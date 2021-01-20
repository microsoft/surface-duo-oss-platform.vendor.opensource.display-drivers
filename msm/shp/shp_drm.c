/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"[drm-shp] %s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_irq.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include "sde_connector.h"
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_atomic.h>
#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>

#include "msm_drv.h"
#include "msm_kms.h"
#include "sde_connector.h"
#include "sde_encoder.h"
#include "sde_crtc.h"
#include "sde_plane.h"
#include "shp_drm.h"

#ifdef CONFIG_DRM_SDE_SHD
#include "../shd/shd_drm.h"
#else
static u32 shd_get_shared_crtc_mask(struct drm_crtc *crtc)
{
	return drm_crtc_mask(crtc);
}
static void shd_skip_shared_plane_update(struct drm_plane *plane,
	struct drm_crtc *crtc)
{
}
#endif

struct shp_pool {
	struct list_head plane_list;
};

/**
 * enum shp_handoff_state: states of handoff
 * @SHP_HANDOFF_NONE: No handoff requested.
 * @SHP_HANDOFF_SET: Activate other shared planes and keep handoff state.
 *                   set back to none state when other shared plane get
 *                   committed. When detach mode is selected, disable the
 *                   plane immediately and set back to none state.
 * @SHP_HANDOFF_ENABLE_REQ: Request to re-enable plane that is currently
 *                   disabled. Set back to none state automatically.
 */
enum shp_handoff_state {
	SHP_HANDOFF_NONE = 0,
	SHP_HANDOFF_SET,
	SHP_HANDOFF_ENABLE_REQ,
};

static const struct drm_prop_enum_list shp_handoff_state_enum_list[] = {
	{ SHP_HANDOFF_NONE, "none" },
	{ SHP_HANDOFF_SET, "set" },
	{ SHP_HANDOFF_ENABLE_REQ, "enable_req" },
};

struct shp_plane_state {
	enum shp_handoff_state handoff;
	bool active;
	bool skip_update;
	uint32_t possible_crtcs;
};

struct shp_plane {
	struct shp_pool *pool;
	struct list_head head;
	bool is_shared;
	bool detach_handoff;

	struct drm_plane_funcs funcs;
	const struct drm_plane_funcs *funcs_orig;

	struct drm_plane_helper_funcs helper_funcs;
	const struct drm_plane_helper_funcs *helper_funcs_orig;

	struct shp_plane *master;
	struct drm_plane *plane;
	uint32_t default_crtcs;

	struct shp_plane_state new_state;
	struct shp_plane_state state;
};

struct shp_device {
	struct device *device;
	struct drm_device *dev;
	struct shp_pool pools[SSPP_MAX];
	struct shp_plane *planes;
	int num_planes;

	struct drm_property *handoff_prop;
	struct notifier_block notifier;

	struct msm_kms_funcs kms_funcs;
	const struct msm_kms_funcs *orig_kms_funcs;
};

struct shp_device g_shp_device;

static void shp_plane_send_uevent(struct drm_device *dev)
{
	char *envp[2] = {"PLANE_POSSIBLE_CRTCS_UPDATED=1", NULL};

	kobject_uevent_env(&dev->primary->kdev->kobj, KOBJ_CHANGE,
				envp);

	SDE_DEBUG("possible crtcs update uevent\n");
}

static void shp_plane_send_handoff_uevent(struct drm_plane *plane)
{
	char name[SZ_32];
	char *envp[2];

	snprintf(name, sizeof(name), "PLANE_HANDOFF_REQUESTED=%d",
			DRMID(plane));

	envp[0] = name;
	envp[1] = NULL;
	kobject_uevent_env(&plane->dev->primary->kdev->kobj,
			KOBJ_CHANGE, envp);

	SDE_DEBUG("%s\n", name);
	SDE_EVT32(DRMID(plane));
}

static void shp_plane_send_enable_req(struct shp_plane *splane)
{
	struct shp_pool *pool = splane->pool;
	struct shp_plane *p;

	list_for_each_entry(p, &pool->plane_list, head) {
		if (p != p->master || p == splane)
			continue;

		if (p->plane->possible_crtcs)
			shp_plane_send_handoff_uevent(p->plane);
	}
}

static int shp_plane_validate(struct shp_plane *splane,
		struct drm_plane_state *state)
{
	struct drm_plane *plane = splane->plane;
	struct shp_pool *pool = splane->pool;
	struct shp_plane_state *shp_state = &splane->new_state;
	struct drm_plane_state *plane_state;
	struct shp_plane *p;
	struct shp_plane_state *pstate;
	int ret;

	if (!shp_state->possible_crtcs) {
		SDE_DEBUG("possible_crtcs is updated\n");
		return 0;
	}

	if (shp_state->handoff == SHP_HANDOFF_ENABLE_REQ) {
		SDE_DEBUG("plane already enabled\n");
		shp_state->handoff = SHP_HANDOFF_NONE;
	}

	if (!shp_state->active) {
		/* handoff only if plane is staged */
		if (!state->crtc && !shp_state->handoff)
			return 0;

		list_for_each_entry(p, &pool->plane_list, head) {
			plane_state = drm_atomic_get_plane_state(
				state->state, p->plane);
			if (IS_ERR(plane_state))
				return PTR_ERR(plane_state);

			pstate = &p->new_state;
			if (p->master == splane) {
				pstate->possible_crtcs = p->default_crtcs;
				pstate->active = true;
				SDE_DEBUG("plane%d set to active",
					p->plane->base.id);
				SDE_EVT32(DRMID(p->plane),
					DRMID(plane_state->crtc),
					pstate->possible_crtcs);
				continue;
			}

			if (pstate->active) {
				if (p == p->master && !pstate->handoff) {
					SDE_ERROR("plane%d is busy\n",
						p->plane->base.id);
					return -EBUSY;
				}

				/* skip update for seamless handoff */
				if (p->plane->state->crtc && state->crtc)
					pstate->skip_update = true;
			}

			ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
			if (ret)
				return ret;

			drm_atomic_set_fb_for_plane(plane_state, NULL);

			pstate->possible_crtcs = 0;
			pstate->active = false;
			pstate->handoff = SHP_HANDOFF_NONE;

			SDE_DEBUG("plane%d: 0x%x h=%d c=%d a=%d\n",
				p->plane->base.id,
				pstate->possible_crtcs,
				pstate->handoff,
				plane_state->crtc ?
					plane_state->crtc->base.id : 0,
				pstate->active);

			SDE_EVT32(DRMID(p->plane),
				DRMID(p->plane->state->crtc),
				DRMID(plane_state->crtc),
				p->plane->possible_crtcs,
				p->state.active);
		}
	}

	if (WARN_ON(!shp_state->active))
		return -EINVAL;

	if (shp_state->handoff != splane->state.handoff ||
			state->crtc != plane->state->crtc) {
		uint32_t crtc_mask;

		if (splane->detach_handoff) {
			if (!shp_state->handoff)
				return 0;

			if (state->crtc) {
				SDE_ERROR("can't handoff when crtc is set\n");
				return -EINVAL;
			}

			shp_state->active = false;
			shp_state->handoff = SHP_HANDOFF_NONE;
			crtc_mask = 0xFFFFFFFF;
		} else if (!shp_state->handoff) {
			crtc_mask = 0;
		} else if (state->crtc) {
			crtc_mask = shd_get_shared_crtc_mask(state->crtc);
		} else {
			crtc_mask = 0xFFFFFFFF;
		}

		list_for_each_entry(p, &pool->plane_list, head) {
			plane_state = drm_atomic_get_plane_state(
				state->state, p->plane);
			if (IS_ERR(plane_state))
				return PTR_ERR(plane_state);

			pstate = &p->new_state;
			if (p->master != splane) {
				pstate->possible_crtcs =
					crtc_mask & p->default_crtcs;
			} else if (!shp_state->active) {
				pstate->possible_crtcs = 0;
				pstate->active = false;
			}

			SDE_DEBUG("plane%d: 0x%x h=%d c=%d a=%d\n",
				p->plane->base.id,
				pstate->possible_crtcs,
				pstate->handoff,
				plane_state->crtc ?
					plane_state->crtc->base.id : 0,
				pstate->active);

			SDE_EVT32(DRMID(p->plane),
				DRMID(p->plane->state->crtc),
				DRMID(plane_state->crtc),
				p->plane->possible_crtcs,
				p->state.active,
				pstate->possible_crtcs,
				pstate->active);
		}
	}

	return 0;
}

static int shp_atomic_check(struct drm_atomic_state *state)
{
	struct shp_device *shp_dev = &g_shp_device;
	struct shp_plane *shp_plane;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	uint32_t plane_mask = 0;
	int i, ret;

	for_each_new_plane_in_state(state, plane, plane_state, i) {
		shp_plane = &shp_dev->planes[plane->index];

		if (!shp_plane->is_shared || shp_plane != shp_plane->master)
			continue;

		if (!plane->possible_crtcs)
			continue;

		plane_mask |= (1 << plane->index);
	}

	if (!plane_mask)
		return 0;

	drm_for_each_plane_mask(plane, state->dev, plane_mask) {
		shp_plane = &shp_dev->planes[plane->index];

		plane_state = drm_atomic_get_existing_plane_state(
			state, plane);

		ret = shp_plane_validate(shp_plane, plane_state);
		if (ret)
			return ret;
	}

	return 0;
}

static int shp_kms_atomic_check(struct msm_kms *kms,
			struct drm_atomic_state *state)
{
	struct shp_device *shp_dev = &g_shp_device;
	int ret;

	ret = shp_atomic_check(state);
	if (ret)
		return ret;

	ret = shp_dev->orig_kms_funcs->atomic_check(kms, state);
	if (ret)
		return ret;

	return 0;
}

static void shp_kms_post_swap(struct msm_kms *kms,
			struct drm_atomic_state *state)
{
	struct shp_device *shp_dev = &g_shp_device;
	struct shp_plane *shp_plane;
	struct drm_plane_state *old_plane_state, *new_plane_state;
	struct drm_plane *plane;
	struct shp_plane_state *new_state;
	bool update = false;
	int i;

	for_each_oldnew_plane_in_state(state, plane, old_plane_state,
				new_plane_state, i) {
		shp_plane = &shp_dev->planes[plane->index];

		if (!shp_plane->is_shared)
			continue;

		new_state = &shp_plane->new_state;

		if (plane->possible_crtcs != new_state->possible_crtcs) {
			SDE_DEBUG("plane%d possible_crtcs 0x%x to 0x%x\n",
				plane->base.id,
				plane->possible_crtcs,
				new_state->possible_crtcs);
			SDE_EVT32(DRMID(plane),
				DRMID(old_plane_state->crtc),
				DRMID(new_plane_state->crtc),
				plane->possible_crtcs,
				new_state->possible_crtcs);
			plane->possible_crtcs = new_state->possible_crtcs;
			update = true;
		}

		if (shp_plane->state.handoff != new_state->handoff) {
			SDE_DEBUG("plane%d handoff %d to %d\n",
				plane->base.id,
				shp_plane->state.handoff,
				new_state->handoff);
			SDE_EVT32(DRMID(plane),
				shp_plane->state.handoff,
				new_state->handoff);
			if (new_state->handoff == SHP_HANDOFF_ENABLE_REQ)
				shp_plane_send_enable_req(shp_plane);
			else
				shp_plane->state.handoff = new_state->handoff;
		}

		if (shp_plane->state.active != new_state->active) {
			SDE_DEBUG("plane%d active %d\n",
				plane->base.id,
				new_state->active);
			SDE_EVT32(DRMID(plane),
				DRMID(old_plane_state->crtc),
				DRMID(new_plane_state->crtc),
				shp_plane->state.active,
				new_state->active,
				new_state->skip_update);
			shp_plane->state.active = new_state->active;
			if (new_state->skip_update) {
				shd_skip_shared_plane_update(plane,
					old_plane_state->crtc);
				SDE_DEBUG("plane%d skip detach\n",
					plane->base.id);
			}
		}

		shp_plane->state.skip_update = new_state->skip_update;
	}

	if (shp_dev->orig_kms_funcs->prepare_fence)
		shp_dev->orig_kms_funcs->prepare_fence(kms, state);

	if (update)
		shp_plane_send_uevent(state->dev);
}

static int shp_plane_atomic_set_property(struct drm_plane *plane,
				   struct drm_plane_state *state,
				   struct drm_property *property,
				   uint64_t val)
{
	struct shp_device *shp_dev = &g_shp_device;
	struct shp_plane *shp_plane;

	shp_plane = &shp_dev->planes[plane->index];

	if (property == shp_dev->handoff_prop) {
		shp_plane->new_state.handoff = val;
		return 0;
	}

	return shp_plane->funcs_orig->atomic_set_property(plane,
			state, property, val);
}

static int shp_plane_atomic_get_property(struct drm_plane *plane,
				   const struct drm_plane_state *state,
				   struct drm_property *property,
				   uint64_t *val)
{
	struct shp_device *shp_dev = &g_shp_device;
	struct shp_plane *shp_plane;

	shp_plane = &shp_dev->planes[plane->index];

	if (property == shp_dev->handoff_prop) {
		*val = shp_plane->new_state.handoff;
		return 0;
	}

	return shp_plane->funcs_orig->atomic_get_property(plane,
			state, property, val);
}

static struct drm_plane_state *shp_plane_atomic_duplicate_state(
				struct drm_plane *plane)
{
	struct shp_device *shp_dev = &g_shp_device;
	struct shp_plane *shp_plane;
	struct shp_plane_state *new_state, *old_state;

	shp_plane = &shp_dev->planes[plane->index];

	if (shp_plane->is_shared) {
		new_state = &shp_plane->new_state;
		old_state = &shp_plane->state;

		new_state->possible_crtcs = plane->possible_crtcs;
		new_state->handoff = old_state->handoff;
		new_state->active = old_state->active;
		new_state->skip_update = false;
	}

	return shp_plane->funcs_orig->atomic_duplicate_state(plane);
}

static void shp_plane_atomic_update(struct drm_plane *plane,
				struct drm_plane_state *old_state)
{
	struct shp_device *shp_dev = &g_shp_device;
	struct shp_plane *shp_plane;

	shp_plane = &shp_dev->planes[plane->index];

	if (shp_plane->is_shared) {
		/* skip h/w update for seamless handoff */
		if (shp_plane->state.skip_update) {
			SDE_DEBUG("skip plane%d update\n",
				plane->base.id);
			return;
		}
	}

	return shp_plane->helper_funcs_orig->atomic_update(plane, old_state);
}

static int shp_parse(struct shp_device *shp)
{
	struct drm_device *dev = shp->dev;
	struct msm_drm_private *priv = dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	struct device_node *of_node, *parent_node;
	struct shp_plane *shp_plane, *parent, *p;
	struct shp_plane_state *shp_state;
	struct drm_plane *plane;
	enum sde_sspp sspp;
	const char *name;
	int dup_count, system_count, total_count, i, j;
	int rc = 0;

	parent_node = of_get_child_by_name(shp->device->of_node,
			"qcom,add-planes");
	if (!parent_node) {
		SDE_ERROR("no planes defined\n");
		return -ENODEV;
	}

	dup_count = of_get_child_count(parent_node) * 2;
	if (!dup_count) {
		SDE_ERROR("no duplicated planes defined\n");
		return -EINVAL;
	}

	pm_runtime_get_sync(sde_kms->dev->dev);

	system_count = priv->num_planes;
	total_count = dup_count + system_count;
	shp->planes = devm_kzalloc(shp->device,
			sizeof(*shp_plane) * total_count, GFP_KERNEL);
	if (!shp->planes) {
		rc = -ENOMEM;
		goto out;
	}

	/* init sspp pool */
	for (i = 0; i < SSPP_MAX; i++)
		INIT_LIST_HEAD(&shp->pools[i].plane_list);

	/* init swap state function */
	shp->orig_kms_funcs = priv->kms->funcs;
	shp->kms_funcs = *priv->kms->funcs;
	shp->kms_funcs.prepare_fence = shp_kms_post_swap;
	shp->kms_funcs.atomic_check = shp_kms_atomic_check;
	priv->kms->funcs = &shp->kms_funcs;

	/* add shared planes */
	shp->num_planes = system_count;
	for_each_child_of_node(parent_node, of_node) {
		rc = of_property_read_string(of_node,
				"qcom,plane-parent", &name);
		if (rc) {
			SDE_ERROR("failed to get parent name\n");
			goto out;
		}

		parent = NULL;
		for (i = 0; i < system_count; i++) {
			if (!strcmp(priv->planes[i]->name, name)) {
				parent = &shp->planes[i];
				parent->plane = priv->planes[i];
				BUG_ON(parent->plane->index != i);
				break;
			}
		}

		if (!parent) {
			SDE_ERROR("parent name %s is not found\n", name);
			rc = -EINVAL;
			goto out;
		}

		if (is_sde_plane_virtual(parent->plane)) {
			SDE_ERROR("virtual plane %s can't set as parent\n",
				name);
			rc = -EINVAL;
			goto out;
		}

		rc = of_property_read_string(of_node,
				"qcom,plane-name", &name);
		if (rc) {
			SDE_ERROR("failed to get plane name\n");
			goto out;
		}

		/* init shared parent */
		if (!parent->is_shared) {
			parent->is_shared = true;
			parent->master = parent;
			parent->state.active = true;
			parent->detach_handoff = true;
			parent->default_crtcs =
				parent->plane->possible_crtcs;
			sspp = sde_plane_pipe(parent->plane);
			parent->pool = &shp->pools[sspp];
			list_add_tail(&parent->head,
				&parent->pool->plane_list);
			drm_object_attach_property(&parent->plane->base,
				shp->handoff_prop, 0);

			/* init virtual plane */
			for (j = i + 1; j < system_count; j++) {
				if (sspp == sde_plane_pipe(priv->planes[j])) {
					p = &shp->planes[j];
					p->plane = priv->planes[j];
					p->is_shared = true;
					p->master = parent;
					p->state.active = true;
					p->default_crtcs =
						p->plane->possible_crtcs;
					p->pool = parent->pool;
					list_add_tail(&p->head,
						&parent->pool->plane_list);
					break;
				}
			}
		}

		plane = sde_plane_init(dev, sspp,
				parent->plane->type,
				parent->plane->possible_crtcs, 0);

		if (!plane) {
			SDE_ERROR("failed to init plane %d\n", plane->index);
			rc = -EINVAL;
			goto out;
		}

		/* create plane */
		plane->possible_crtcs = 0;
		plane->name = kasprintf(GFP_KERNEL, "%s", name);
		BUG_ON(plane->index != shp->num_planes);
		shp_plane = &shp->planes[shp->num_planes++];
		shp_plane->plane = plane;
		shp_plane->master = shp_plane;
		shp_plane->pool = parent->pool;
		shp_plane->default_crtcs = parent->plane->possible_crtcs;
		list_add_tail(&shp_plane->head, &shp_plane->pool->plane_list);
		drm_object_attach_property(&shp_plane->plane->base,
				shp->handoff_prop, 0);
		shp_plane->is_shared = true;

		/* init shared plane state */
		shp_state = &shp_plane->state;
		shp_state->handoff = of_property_read_bool(of_node,
				"qcom,plane-init-handoff");
		shp_state->active = of_property_read_bool(of_node,
				"qcom,plane-init-active");
		shp_plane->detach_handoff = of_property_read_bool(of_node,
				"qcom,plane-detach-handoff");

		if (plane->funcs->reset)
			plane->funcs->reset(plane);

		if (of_property_read_string(of_node,
				"qcom,plane-virt-name", &name))
			continue;

		/* create virtual plane */
		plane = sde_plane_init(dev, sspp,
				parent->plane->type,
				parent->plane->possible_crtcs, plane->base.id);

		if (!plane) {
			SDE_ERROR("failed to init plane %d\n", plane->index);
			rc = -EINVAL;
			goto out;
		}

		plane->possible_crtcs = 0;
		plane->name = kasprintf(GFP_KERNEL, "%s", name);
		BUG_ON(plane->index != shp->num_planes);
		p = shp_plane;
		shp_plane = &shp->planes[shp->num_planes++];
		shp_plane->plane = plane;
		shp_plane->master = p;
		shp_plane->pool = parent->pool;
		shp_plane->default_crtcs = parent->plane->possible_crtcs;
		list_add_tail(&shp_plane->head, &shp_plane->pool->plane_list);
		shp_plane->is_shared = true;
		shp_plane->state.active = p->state.active;
	}

	/* setup all planes with new atomic check */
	for (i = 0; i < shp->num_planes; i++) {
		shp_plane = &shp->planes[i];
		if (!shp_plane->is_shared)
			continue;

		shp_plane->funcs = *shp_plane->plane->funcs;
		shp_plane->funcs_orig = shp_plane->plane->funcs;
		shp_plane->funcs.atomic_set_property =
				shp_plane_atomic_set_property;
		shp_plane->funcs.atomic_get_property =
				shp_plane_atomic_get_property;
		shp_plane->funcs.atomic_duplicate_state =
				shp_plane_atomic_duplicate_state;
		shp_plane->plane->funcs = &shp_plane->funcs;

		shp_plane->helper_funcs =
				*shp_plane->plane->helper_private;
		shp_plane->helper_funcs_orig =
				shp_plane->plane->helper_private;
		shp_plane->helper_funcs.atomic_update =
				shp_plane_atomic_update;
		shp_plane->plane->helper_private = &shp_plane->helper_funcs;
	}

	/* update init-active cases */
	for (i = system_count; i < shp->num_planes; i++) {
		shp_plane = &shp->planes[i];
		shp_state = &shp_plane->state;

		if (!shp_state->active || shp_plane->master != shp_plane)
			continue;

		/* update possible crtcs */
		list_for_each_entry(p, &shp_plane->pool->plane_list, head) {
			if (shp_state->handoff || p->master == shp_plane)
				p->plane->possible_crtcs = p->default_crtcs;
			else
				p->plane->possible_crtcs = 0;

			if (p->master != shp_plane)
				p->state.active = false;
		}
	}

out:
	pm_runtime_put_sync(sde_kms->dev->dev);

	if (!shp->planes)
		return rc;

	/* dump all the planes */
	for (i = 0; i < shp->num_planes; i++) {
		shp_plane = &shp->planes[i];
		if (!shp_plane->is_shared)
			continue;

		shp_state = &shp_plane->state;
		SDE_DEBUG("%s[%d]: 0x%x/0x%x v=%d h=%d a=%d\n",
			shp_plane->plane->name,
			shp_plane->plane->base.id,
			shp_plane->plane->possible_crtcs,
			shp_plane->default_crtcs,
			is_sde_plane_virtual(shp_plane->plane),
			shp_state->handoff,
			shp_state->active);
	}

	return rc;
}

static int sde_shp_notifier(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct shp_device *shp_dev;
	int ret;

	if (action != MSM_COMP_OBJECT_CREATED)
		return 0;

	shp_dev = container_of(nb, struct shp_device, notifier);

	ret = shp_parse(shp_dev);
	if (ret) {
		SDE_ERROR("failed to parse shared plane device\n");
		return ret;
	}

	return 0;
}

static int sde_shp_bind(struct device *dev, struct device *master,
		void *data)
{
	int rc = 0;
	struct shp_device *shp_dev;
	struct drm_device *drm;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev || !master) {
		pr_err("invalid param(s), dev %pK, pdev %pK, master %pK\n",
				dev, pdev, master);
		rc = -EINVAL;
		goto end;
	}

	drm = dev_get_drvdata(master);
	shp_dev = platform_get_drvdata(pdev);
	if (!drm || !shp_dev) {
		pr_err("invalid param(s), drm %pK, shp_dev %pK\n",
				drm, shp_dev);
		rc = -EINVAL;
		goto end;
	}

	shp_dev->handoff_prop = drm_property_create_enum(drm,
			DRM_MODE_PROP_ENUM, "handoff",
			shp_handoff_state_enum_list,
			ARRAY_SIZE(shp_handoff_state_enum_list));

	if (!shp_dev->handoff_prop)
		return -ENOMEM;

	shp_dev->dev = drm;
	shp_dev->notifier.notifier_call = sde_shp_notifier;

	rc = msm_drm_register_component(drm, &shp_dev->notifier);
	if (rc) {
		pr_err("failed to register component notifier\n");
		goto end;
	}

end:
	return rc;
}

static void sde_shp_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct shp_device *shp_dev;
	struct platform_device *pdev = to_platform_device(dev);

	if (!dev || !pdev) {
		pr_err("invalid param");
		return;
	}

	shp_dev = platform_get_drvdata(pdev);
	if (!shp_dev) {
		pr_err("invalid param");
		return;
	}

	msm_drm_unregister_component(shp_dev->dev, &shp_dev->notifier);
}

static const struct component_ops sde_shp_comp_ops = {
	.bind = sde_shp_bind,
	.unbind = sde_shp_unbind,
};

static int sde_shp_probe(struct platform_device *pdev)
{
	struct shp_device *shp_dev;
	int ret;

	shp_dev = &g_shp_device;
	if (shp_dev->dev) {
		SDE_ERROR("only single device is supported\n");
		return -EEXIST;
	}

	shp_dev->device = &pdev->dev;
	platform_set_drvdata(pdev, shp_dev);

	ret = component_add(&pdev->dev, &sde_shp_comp_ops);
	if (ret) {
		pr_err("component add failed, rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int sde_shp_remove(struct platform_device *pdev)
{
	struct shp_plane *shd_dev;

	shd_dev = platform_get_drvdata(pdev);
	if (!shd_dev)
		return 0;

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,sde-shared-plane"},
	{},
};

static struct platform_driver sde_shp_driver = {
	.probe = sde_shp_probe,
	.remove = sde_shp_remove,
	.driver = {
		.name = "sde_shp",
		.of_match_table = dt_match,
		.suppress_bind_attrs = true,
	},
};

void __init sde_shp_register(void)
{
	platform_driver_register(&sde_shp_driver);
}

void __exit sde_shp_unregister(void)
{
	platform_driver_unregister(&sde_shp_driver);
}
