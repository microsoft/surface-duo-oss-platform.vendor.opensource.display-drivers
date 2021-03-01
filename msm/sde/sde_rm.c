// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s] " fmt, __func__
#include "sde_kms.h"
#include "sde_hw_lm.h"
#include "sde_hw_ctl.h"
#include "sde_hw_cdm.h"
#include "sde_hw_dspp.h"
#include "sde_hw_ds.h"
#include "sde_hw_pingpong.h"
#include "sde_hw_intf.h"
#include "sde_hw_wb.h"
#include "sde_encoder.h"
#include "sde_connector.h"
#include "sde_hw_dsc.h"
#include "sde_hw_rot.h"
#include "sde_crtc.h"
#include "sde_hw_qdss.h"
#include "sde_hw_roi_misr.h"

#define RESERVED_BY_OTHER(h, e) \
	((h)->enc_id && ((h)->enc_id != (e)))

#define RM_RQ_LOCK(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_LOCK))
#define RM_RQ_CLEAR(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_RESERVE_CLEAR))
#define RM_RQ_DSPP(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_DSPP))
#define RM_RQ_DS(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_DS))
#define RM_RQ_CWB(r) ((r)->top_ctrl & BIT(SDE_RM_TOPCTL_CWB))
#define RM_IS_TOPOLOGY_MATCH(t, r) ((t).num_lm == (r).num_lm && \
				(t).num_comp_enc == (r).num_enc && \
				(t).num_intf == (r).num_intf)

/**
 * toplogy information to be used when ctl path version does not
 * support driving more than one interface per ctl_path
 */
static const struct sde_rm_topology_def g_top_table[] = {
	{   SDE_RM_TOPOLOGY_NONE,                 0, 0, 0, 0, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE,           1, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE_DSC,       1, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE,             2, 0, 2, 2, true  },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSC,         2, 2, 2, 2, true  },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE,     2, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC, 2, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE,    2, 2, 1, 1, false },
	{   SDE_RM_TOPOLOGY_PPSPLIT,              1, 0, 2, 1, true  },
};

/**
 * topology information to be used when the ctl path version
 * is SDE_CTL_CFG_VERSION_1_0_0
 */
static const struct sde_rm_topology_def g_ctl_ver_1_top_table[] = {
	{   SDE_RM_TOPOLOGY_NONE,                 0, 0, 0, 0, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE,           1, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_SINGLEPIPE_DSC,       1, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE,             2, 0, 2, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSC,         2, 2, 2, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE,     2, 0, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE_DSC, 2, 1, 1, 1, false },
	{   SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE,    2, 2, 1, 1, false },
	{   SDE_RM_TOPOLOGY_PPSPLIT,              1, 0, 2, 1, false },
	{   SDE_RM_TOPOLOGY_TRIPLEPIPE,           3, 0, 3, 1, false },
	{   SDE_RM_TOPOLOGY_TRIPLEPIPE_DSC,       3, 3, 3, 1, false },
	{   SDE_RM_TOPOLOGY_QUADPIPE_3DMERGE,     4, 0, 2, 1, false },
	{   SDE_RM_TOPOLOGY_QUADPIPE_3DMERGE_DSC, 4, 3, 2, 1, false },
	{   SDE_RM_TOPOLOGY_QUADPIPE_DSCMERGE,    4, 4, 2, 1, false },
	{   SDE_RM_TOPOLOGY_SIXPIPE_3DMERGE,      6, 0, 3, 1, false },
	{   SDE_RM_TOPOLOGY_SIXPIPE_DSCMERGE,     6, 6, 3, 1, false },
};


/**
 * struct sde_rm_requirements - Reservation requirements parameter bundle
 * @top_ctrl:  topology control preference from kernel client
 * @top:       selected topology for the display
 * @hw_res:	   Hardware resources required as reported by the encoders
 */
struct sde_rm_requirements {
	uint64_t top_ctrl;
	const struct sde_rm_topology_def *topology;
	struct sde_encoder_hw_resources hw_res;
};

/**
 * struct sde_rm_hw_blk - hardware block tracking list member
 * @list:	List head for list of all hardware blocks tracking items
 * @enc_id:	Reservations are tracked by Encoder DRM object ID.
 *		CRTCs may be connected to multiple Encoders.
 *		An encoder or connector id identifies the display path.
 * @ext_hw:	Flag for external created HW block
 * @type:	Type of hardware block this structure tracks
 * @id:		Hardware ID number, within it's own space, ie. LM_X
 * @catalog:	Pointer to the hardware catalog entry for this block
 * @hw:		Pointer to the hardware register access object for this block
 * @uuid:	HW unique id
 */
struct sde_rm_hw_blk {
	struct list_head list;
	uint32_t enc_id;
	bool ext_hw;
	enum sde_hw_blk_type type;
	uint32_t id;
	struct sde_hw_blk *hw;
	uint32_t uuid;
};

/**
 * struct sde_rm_state - SDE dynamic hardware resource manager state
 * @base: private state base
 * @rm: sde_rm handle
 * @hw_blks: array of lists of hardware resources present in the system, one
 *	list per type of hardware block
 */
struct sde_rm_state {
	struct drm_private_state base;
	struct list_head hw_blks[SDE_HW_BLK_MAX];
};

/**
 * sde_rm_dbg_rsvp_stage - enum of steps in making reservation for event logging
 */
enum sde_rm_dbg_rsvp_stage {
	SDE_RM_STAGE_BEGIN,
	SDE_RM_STAGE_AFTER_CLEAR,
	SDE_RM_STAGE_AFTER_RSVPNEXT,
};

#define to_sde_rm_priv_state(x) \
		container_of((x), struct sde_rm_state, base)

static void _sde_rm_print_rsvps(
		struct sde_rm_state *state,
		enum sde_rm_dbg_rsvp_stage stage)
{
	struct sde_rm_hw_blk *blk;
	enum sde_hw_blk_type type;

	SDE_DEBUG("%d state=%pK\n", stage, state);

	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry(blk, &state->hw_blks[type], list) {
			if (!blk->enc_id)
				continue;

			SDE_DEBUG("%d rsvp[e%u] %d %d\n", stage,
				blk->enc_id,
				blk->type, blk->id);

			SDE_EVT32(stage,
				blk->enc_id,
				blk->type, blk->id);
		}
	}
}

static void sde_rm_destroy_state(struct drm_private_obj *obj,
		struct drm_private_state *base_state)
{
	struct sde_rm_state *state = to_sde_rm_priv_state(base_state);
	struct sde_rm_hw_blk *hw_blk, *tmp_hw_blk;
	int i;

	for (i = 0; i < SDE_HW_BLK_MAX; i++) {
		list_for_each_entry_safe(hw_blk, tmp_hw_blk,
				&state->hw_blks[i], list) {
			kfree(hw_blk);
		}
	}

	kfree(state);
}

static struct drm_private_state *sde_rm_duplicate_state(
		struct drm_private_obj *obj)
{
	struct sde_rm_state *state, *old_state =
			to_sde_rm_priv_state(obj->state);
	struct sde_rm_hw_blk *hw_blk, *old_hw_blk;
	int i;

	state = kmemdup(old_state, sizeof(*state), GFP_KERNEL);
	if (!state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &state->base);

	for (i = 0; i < SDE_HW_BLK_MAX; i++) {
		INIT_LIST_HEAD(&state->hw_blks[i]);
		list_for_each_entry(old_hw_blk, &old_state->hw_blks[i], list) {
			hw_blk = kmemdup(old_hw_blk, sizeof(*hw_blk),
					GFP_KERNEL);
			if (!hw_blk)
				goto bail;
			list_add_tail(&hw_blk->list, &state->hw_blks[i]);
		}
	}

	return &state->base;

bail:
	sde_rm_destroy_state(obj, &state->base);
	return NULL;
}

static const struct drm_private_state_funcs sde_rm_state_funcs = {
	.atomic_duplicate_state = sde_rm_duplicate_state,
	.atomic_destroy_state = sde_rm_destroy_state,
};

static struct sde_rm_state *sde_rm_get_atomic_state(
		struct drm_atomic_state *state, struct sde_rm *rm)
{
	struct drm_device *dev = rm->dev;

	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));

	return to_sde_rm_priv_state(
			drm_atomic_get_private_obj_state(state, &rm->obj));
}

static uint64_t sde_rm_get_enc_res_mask(struct sde_rm *rm,
		struct sde_rm_state *state,
		struct drm_encoder *drm_enc)
{
	struct sde_rm_hw_blk *hw_blk;
	uint64_t mask = 0LL;
	int i;

	for (i = 0; i < SDE_HW_BLK_MAX; i++) {
		list_for_each_entry(hw_blk, &state->hw_blks[i], list) {
			if (drm_enc->base.id == hw_blk->enc_id &&
					!hw_blk->ext_hw)
				mask |= 1LL << hw_blk->uuid;
		}
	}

	return mask;
}

uint64_t sde_rm_atomic_get_resource_mask(struct sde_rm *rm,
		struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *old_crtc_state, *new_crtc_state;
	struct drm_encoder *encoder;
	struct sde_rm_state *old_rm_state, *new_rm_state;
	uint64_t mask = 0;
	int i;

	for_each_oldnew_crtc_in_state(state, crtc, old_crtc_state, new_crtc_state, i) {
		if (!drm_atomic_crtc_needs_modeset(new_crtc_state))
			continue;

		new_rm_state = sde_rm_get_atomic_state(state, rm);
		if (IS_ERR(new_rm_state))
			return 0;
		old_rm_state = to_sde_rm_priv_state(rm->obj.state);

		drm_for_each_encoder_mask(encoder, state->dev,
				new_crtc_state->encoder_mask)
			mask |= sde_rm_get_enc_res_mask(rm, new_rm_state,
				encoder);

		drm_for_each_encoder_mask(encoder, state->dev,
				old_crtc_state->encoder_mask)
			mask |= sde_rm_get_enc_res_mask(rm, old_rm_state,
				encoder);
	}

	return mask;
}

struct sde_hw_mdp *sde_rm_get_mdp(struct sde_rm *rm)
{
	return rm->hw_mdp;
}

void sde_rm_init_hw_iter(
		struct sde_rm_hw_iter *iter,
		uint32_t enc_id,
		enum sde_hw_blk_type type)
{
	memset(iter, 0, sizeof(*iter));
	iter->enc_id = enc_id;
	iter->type = type;
}

enum sde_rm_topology_name sde_rm_get_topology_name(struct sde_rm *rm,
	struct msm_display_topology topology)
{
	int i;

	for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++)
		if (RM_IS_TOPOLOGY_MATCH(rm->topology_tbl[i], topology))
			return rm->topology_tbl[i].top_name;

	return SDE_RM_TOPOLOGY_NONE;
}

int sde_rm_get_topology_num_encoders(struct sde_rm *rm,
	enum sde_rm_topology_name topology)
{
	int i;

	for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++)
		if (rm->topology_tbl[i].top_name == topology)
			return rm->topology_tbl[i].num_comp_enc;

	return 0;
}

static bool sde_rm_is_dscmerge_case(enum sde_rm_topology_name top_name)
{
	return (top_name == SDE_RM_TOPOLOGY_DUALPIPE_DSCMERGE
			|| top_name == SDE_RM_TOPOLOGY_QUADPIPE_DSCMERGE
			|| top_name == SDE_RM_TOPOLOGY_SIXPIPE_DSCMERGE);
}

int sde_rm_get_roi_misr_num(struct sde_rm *rm,
		enum sde_rm_topology_name topology)
{
	int i;

	for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++)
		if (rm->topology_tbl[i].top_name == topology) {
			if (topology == SDE_RM_TOPOLOGY_PPSPLIT)
				return 0;
			else if (sde_rm_is_dscmerge_case(topology))
				return rm->topology_tbl[i].num_intf * 2;
			else
				return rm->topology_tbl[i].num_intf;
		}

	return 0;
}

static bool _sde_rm_get_hw_locked(struct sde_rm *rm,
		struct sde_rm_state *state,
		struct sde_rm_hw_iter *i)
{
	struct list_head *blk_list;

	if (!rm || !i || i->type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid rm\n");
		return false;
	}

	i->hw = NULL;
	blk_list = &state->hw_blks[i->type];

	if (i->blk && (&i->blk->list == blk_list)) {
		SDE_DEBUG("attempt resume iteration past last\n");
		return false;
	}

	i->blk = list_prepare_entry(i->blk, blk_list, list);

	list_for_each_entry_continue(i->blk, blk_list, list) {
		if (i->blk->type != i->type) {
			SDE_ERROR("found incorrect block type %d on %d list\n",
					i->blk->type, i->type);
			return false;
		}

		if ((i->enc_id == 0) || (i->blk->enc_id == i->enc_id)) {
			i->hw = i->blk->hw;
			SDE_DEBUG("found type %d id %d for enc %d\n",
					i->type, i->blk->id, i->enc_id);
			return true;
		}
	}

	SDE_DEBUG("no match, type %d for enc %d\n", i->type, i->enc_id);

	return false;
}

bool sde_rm_request_hw_blk(struct sde_rm *rm,
		struct sde_rm_hw_request *hw_blk_info)
{
	struct list_head *blk_list;
	struct sde_rm_hw_blk *blk = NULL;
	struct sde_rm_state *state;

	if (!rm || !hw_blk_info || hw_blk_info->type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid rm\n");
		return false;
	}

	hw_blk_info->hw = NULL;
	state = to_sde_rm_priv_state(rm->obj.state);
	blk_list = &state->hw_blks[hw_blk_info->type];

	blk = list_prepare_entry(blk, blk_list, list);

	list_for_each_entry_continue(blk, blk_list, list) {
		if (blk->type != hw_blk_info->type) {
			SDE_ERROR("found incorrect block type %d on %d list\n",
					blk->type, hw_blk_info->type);
			return false;
		}

		if (blk->hw->id == hw_blk_info->id) {
			hw_blk_info->hw = blk->hw;
			SDE_DEBUG("found type %d id %d\n",
					blk->type, blk->id);
			return true;
		}
	}

	SDE_DEBUG("no match, type %d id %d\n", hw_blk_info->type,
			hw_blk_info->id);

	return false;
}

bool sde_rm_get_hw(struct sde_rm *rm, struct sde_rm_hw_iter *i)
{
	struct sde_rm_state *state;
	bool ret;

	state = to_sde_rm_priv_state(rm->obj.state);
	ret = _sde_rm_get_hw_locked(rm, state, i);

	return ret;
}

bool sde_rm_atomic_get_hw(struct sde_rm *rm,
		struct drm_atomic_state *atomic_state,
		struct sde_rm_hw_iter *i)
{
	struct sde_rm_state *state;
	bool ret;

	state = sde_rm_get_atomic_state(atomic_state, rm);
	if (IS_ERR(state))
		return false;

	ret = _sde_rm_get_hw_locked(rm, state, i);

	return ret;
}

static void _sde_rm_hw_destroy(enum sde_hw_blk_type type, void *hw)
{
	switch (type) {
	case SDE_HW_BLK_LM:
		sde_hw_lm_destroy(hw);
		break;
	case SDE_HW_BLK_DSPP:
		sde_hw_dspp_destroy(hw);
		break;
	case SDE_HW_BLK_DS:
		sde_hw_ds_destroy(hw);
		break;
	case SDE_HW_BLK_CTL:
		sde_hw_ctl_destroy(hw);
		break;
	case SDE_HW_BLK_CDM:
		sde_hw_cdm_destroy(hw);
		break;
	case SDE_HW_BLK_PINGPONG:
		sde_hw_pingpong_destroy(hw);
		break;
	case SDE_HW_BLK_INTF:
		sde_hw_intf_destroy(hw);
		break;
	case SDE_HW_BLK_WB:
		sde_hw_wb_destroy(hw);
		break;
	case SDE_HW_BLK_DSC:
		sde_hw_dsc_destroy(hw);
		break;
	case SDE_HW_BLK_ROI_MISR:
		sde_hw_roi_misr_destroy(hw);
		break;
	case SDE_HW_BLK_ROT:
		sde_hw_rot_destroy(hw);
		break;
	case SDE_HW_BLK_QDSS:
		sde_hw_qdss_destroy(hw);
		break;
	case SDE_HW_BLK_SSPP:
		/* SSPPs are not managed by the resource manager */
	case SDE_HW_BLK_TOP:
		/* Top is a singleton, not managed in hw_blks list */
	case SDE_HW_BLK_MAX:
	default:
		SDE_ERROR("unsupported block type %d\n", type);
		break;
	}
}

int sde_rm_destroy(struct sde_rm *rm)
{
	struct sde_rm_state *state;
	struct sde_rm_hw_blk *hw_cur, *hw_nxt;
	enum sde_hw_blk_type type;

	if (!rm) {
		SDE_ERROR("invalid rm\n");
		return -EINVAL;
	}

	state = to_sde_rm_priv_state(rm->obj.state);

	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry_safe(hw_cur, hw_nxt, &state->hw_blks[type],
				list) {
			list_del(&hw_cur->list);
			_sde_rm_hw_destroy(hw_cur->type, hw_cur->hw);
			kfree(hw_cur);
		}
	}

	sde_hw_mdp_destroy(rm->hw_mdp);
	rm->hw_mdp = NULL;

	return 0;
}

static int _sde_rm_hw_blk_create(
		struct sde_rm *rm,
		struct sde_mdss_cfg *cat,
		void __iomem *mmio,
		enum sde_hw_blk_type type,
		uint32_t id,
		void *hw_catalog_info)
{
	struct sde_rm_hw_blk *blk;
	struct sde_hw_mdp *hw_mdp;
	struct sde_rm_state *state;
	void *hw;

	hw_mdp = rm->hw_mdp;

	switch (type) {
	case SDE_HW_BLK_LM:
		hw = sde_hw_lm_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_DSPP:
		hw = sde_hw_dspp_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_DS:
		hw = sde_hw_ds_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_CTL:
		hw = sde_hw_ctl_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_CDM:
		hw = sde_hw_cdm_init(id, mmio, cat, hw_mdp);
		break;
	case SDE_HW_BLK_PINGPONG:
		hw = sde_hw_pingpong_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_INTF:
		hw = sde_hw_intf_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_WB:
		hw = sde_hw_wb_init(id, mmio, cat, hw_mdp);
		break;
	case SDE_HW_BLK_DSC:
		hw = sde_hw_dsc_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_ROI_MISR:
		hw = sde_hw_roi_misr_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_ROT:
		hw = sde_hw_rot_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_QDSS:
		hw = sde_hw_qdss_init(id, mmio, cat);
		break;
	case SDE_HW_BLK_SSPP:
		/* SSPPs are not managed by the resource manager */
	case SDE_HW_BLK_TOP:
		/* Top is a singleton, not managed in hw_blks list */
	case SDE_HW_BLK_MAX:
	default:
		SDE_ERROR("unsupported block type %d\n", type);
		return -EINVAL;
	}

	if (IS_ERR_OR_NULL(hw)) {
		SDE_ERROR("failed hw object creation: type %d, err %ld\n",
				type, PTR_ERR(hw));
		return -EFAULT;
	}

	blk = kzalloc(sizeof(*blk), GFP_KERNEL);
	if (!blk) {
		_sde_rm_hw_destroy(type, hw);
		return -ENOMEM;
	}

	state = to_sde_rm_priv_state(rm->obj.state);
	blk->type = type;
	blk->id = id;
	blk->hw = hw;
	blk->uuid = rm->next_uuid++;
	list_add_tail(&blk->list, &state->hw_blks[type]);

	/* we're using uint64_t to store uuid mask */
	WARN_ON(rm->next_uuid >= 64);

	return 0;
}

int sde_rm_init(struct sde_rm *rm,
		struct sde_mdss_cfg *cat,
		void __iomem *mmio,
		struct drm_device *dev)
{
	int i, rc = 0;
	struct sde_rm_state *state;
	enum sde_hw_blk_type type;

	if (!rm || !cat || !mmio || !dev) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}

	state = kzalloc(sizeof(*state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	/* Clear, setup lists */
	memset(rm, 0, sizeof(*rm));

	drm_atomic_private_obj_init(dev,
				    &rm->obj,
				    &state->base,
				    &sde_rm_state_funcs);

	for (type = 0; type < SDE_HW_BLK_MAX; type++)
		INIT_LIST_HEAD(&state->hw_blks[type]);

	rm->dev = dev;

	if (IS_SDE_CTL_REV_100(cat->ctl_rev))
		rm->topology_tbl = g_ctl_ver_1_top_table;
	else
		rm->topology_tbl = g_top_table;

	/* Some of the sub-blocks require an mdptop to be created */
	rm->hw_mdp = sde_hw_mdptop_init(MDP_TOP, mmio, cat);
	if (IS_ERR_OR_NULL(rm->hw_mdp)) {
		rc = PTR_ERR(rm->hw_mdp);
		rm->hw_mdp = NULL;
		SDE_ERROR("failed: mdp hw not available\n");
		goto fail;
	}

	/* Interrogate HW catalog and create tracking items for hw blocks */
	for (i = 0; i < cat->mixer_count; i++) {
		struct sde_lm_cfg *lm = &cat->mixer[i];

		if (lm->pingpong == PINGPONG_MAX) {
			SDE_ERROR("mixer %d without pingpong\n", lm->id);
			goto fail;
		}

		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_LM,
				cat->mixer[i].id, &cat->mixer[i]);
		if (rc) {
			SDE_ERROR("failed: lm hw not available\n");
			goto fail;
		}

		if (!rm->lm_max_width) {
			rm->lm_max_width = lm->sblk->maxwidth;
		} else if (rm->lm_max_width != lm->sblk->maxwidth) {
			/*
			 * Don't expect to have hw where lm max widths differ.
			 * If found, take the min.
			 */
			SDE_ERROR("unsupported: lm maxwidth differs\n");
			if (rm->lm_max_width > lm->sblk->maxwidth)
				rm->lm_max_width = lm->sblk->maxwidth;
		}
	}

	for (i = 0; i < cat->dspp_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_DSPP,
				cat->dspp[i].id, &cat->dspp[i]);
		if (rc) {
			SDE_ERROR("failed: dspp hw not available\n");
			goto fail;
		}
	}

	if (cat->mdp[0].has_dest_scaler) {
		for (i = 0; i < cat->ds_count; i++) {
			rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_DS,
					cat->ds[i].id, &cat->ds[i]);
			if (rc) {
				SDE_ERROR("failed: ds hw not available\n");
				goto fail;
			}
		}
	}

	for (i = 0; i < cat->pingpong_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_PINGPONG,
				cat->pingpong[i].id, &cat->pingpong[i]);
		if (rc) {
			SDE_ERROR("failed: pp hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->dsc_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_DSC,
			cat->dsc[i].id, &cat->dsc[i]);
		if (rc) {
			SDE_ERROR("failed: dsc hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->roi_misr_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_ROI_MISR,
			cat->roi_misr[i].id, &cat->roi_misr[i]);
		if (rc) {
			SDE_ERROR("failed: roi misr hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->intf_count; i++) {
		if (cat->intf[i].type == INTF_NONE) {
			SDE_DEBUG("skip intf %d with type none\n", i);
			continue;
		}

		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_INTF,
				cat->intf[i].id, &cat->intf[i]);
		if (rc) {
			SDE_ERROR("failed: intf hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->wb_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_WB,
				cat->wb[i].id, &cat->wb[i]);
		if (rc) {
			SDE_ERROR("failed: wb hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->rot_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_ROT,
				cat->rot[i].id, &cat->rot[i]);
		if (rc) {
			SDE_ERROR("failed: rot hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->ctl_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_CTL,
				cat->ctl[i].id, &cat->ctl[i]);
		if (rc) {
			SDE_ERROR("failed: ctl hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->cdm_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_CDM,
				cat->cdm[i].id, &cat->cdm[i]);
		if (rc) {
			SDE_ERROR("failed: cdm hw not available\n");
			goto fail;
		}
	}

	for (i = 0; i < cat->qdss_count; i++) {
		rc = _sde_rm_hw_blk_create(rm, cat, mmio, SDE_HW_BLK_QDSS,
				cat->qdss[i].id, &cat->qdss[i]);
		if (rc) {
			SDE_ERROR("failed: qdss hw not available\n");
			goto fail;
		}
	}

	return 0;

fail:
	sde_rm_destroy(rm);

	return rc;
}

/**
 * _sde_rm_check_lm_and_get_connected_blks - check if proposed layer mixer meets
 *	proposed use case requirements, incl. hardwired dependent blocks like
 *	pingpong, and dspp.
 * @rm: sde resource manager handle
 * @enc_id: encoder id
 * @reqs: proposed use case requirements
 * @lm: proposed layer mixer, function checks if lm, and all other hardwired
 *      blocks connected to the lm (pp, dspp) are available and appropriate
 * @dspp: output parameter, dspp block attached to the layer mixer.
 *        NULL if dspp was not available, or not matching requirements.
 * @pp: output parameter, pingpong block attached to the layer mixer.
 *      NULL if dspp was not available, or not matching requirements.
 * @roi_misr: output parameter, roi misr block attached to the layer mixer.
 *      NULL if misr was not available, or not matching requirements.
 * @dsc: output parameter, dsc block attached to the layer mixer.
 *      NULL if dsc was not available, or not matching requirements.
 * @primary_lm: if non-null, this function check if lm is compatible primary_lm
 *              as well as satisfying all other requirements
 * @Return: true if lm matches all requirements, false otherwise
 */
static bool _sde_rm_check_lm_and_get_connected_blks(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		struct sde_rm_requirements *reqs,
		struct sde_rm_hw_blk *lm,
		struct sde_rm_hw_blk **dspp,
		struct sde_rm_hw_blk **ds,
		struct sde_rm_hw_blk **pp,
		struct sde_rm_hw_blk **roi_misr,
		struct sde_rm_hw_blk **dsc,
		struct sde_rm_hw_blk *primary_lm)
{
	struct msm_drm_private *priv = rm->dev->dev_private;
	struct sde_kms *sde_kms = to_sde_kms(priv->kms);
	const struct sde_lm_cfg *lm_cfg = to_sde_hw_mixer(lm->hw)->cap;
	const struct sde_pingpong_cfg *pp_cfg;
	struct sde_rm_hw_iter iter;
	bool is_valid_dspp, is_valid_ds, ret;
	u32 display_pref, cwb_pref;

	*dspp = NULL;
	*ds = NULL;
	*pp = NULL;
	*roi_misr = NULL;
	*dsc = NULL;
	display_pref = lm_cfg->features & BIT(SDE_DISP_PRIMARY_PREF);
	cwb_pref = lm_cfg->features & BIT(SDE_DISP_CWB_PREF);

	SDE_DEBUG("check lm %d: dspp %d ds %d pp %d roi_misr %d ",
		lm_cfg->id, lm_cfg->dspp, lm_cfg->ds,
		lm_cfg->pingpong, lm_cfg->roi_misr);
	SDE_DEBUG("disp_pref: %d cwb_pref%d\n",
		display_pref, cwb_pref);

	/* Check if this layer mixer is a peer of the proposed primary LM */
	if (primary_lm) {
		const struct sde_lm_cfg *prim_lm_cfg =
				to_sde_hw_mixer(primary_lm->hw)->cap;

		if (!test_bit(lm_cfg->id, &prim_lm_cfg->lm_pair_mask)) {
			SDE_DEBUG("lm %d not peer of lm %d\n", lm_cfg->id,
					prim_lm_cfg->id);
			return false;
		}
	}

	/* bypass rest of the checks if LM for primary display is found */
	if (!display_pref) {
		is_valid_dspp = (lm_cfg->dspp != DSPP_MAX) ? true : false;
		is_valid_ds = (lm_cfg->ds != DS_MAX) ? true : false;

		/**
		 * RM_RQ_X: specification of which LMs to choose
		 * is_valid_X: indicates whether LM is tied with block X
		 * ret: true if given LM matches the user requirement,
		 *      false otherwise
		 */
		if (RM_RQ_DSPP(reqs) && RM_RQ_DS(reqs))
			ret = (is_valid_dspp && is_valid_ds);
		else if (RM_RQ_DSPP(reqs))
			ret = is_valid_dspp && !is_valid_ds;
		else if (RM_RQ_DS(reqs))
			ret = !is_valid_dspp && is_valid_ds;
		else
			ret = !(is_valid_dspp || is_valid_ds);

		if (!ret) {
			SDE_DEBUG(
				"fail:lm(%d)req_dspp(%d)dspp(%d)req_ds(%d)ds(%d)\n",
				lm_cfg->id, (bool)(RM_RQ_DSPP(reqs)),
				lm_cfg->dspp, (bool)(RM_RQ_DS(reqs)),
				lm_cfg->ds);
			return ret;
		}

		/**
		 * If CWB is enabled and LM is not CWB supported
		 * then return false.
		 */
		if (RM_RQ_CWB(reqs) && !cwb_pref) {
			SDE_DEBUG("fail: cwb supported lm not allocated\n");
			return false;
		}

	} else if (!(reqs->hw_res.is_primary && display_pref)) {
		SDE_DEBUG(
			"display preference is not met. is_primary: %d display_pref: %d\n",
			(int)reqs->hw_res.is_primary, (int)display_pref);
		return false;
	}

	/* Already reserved? */
	if (RESERVED_BY_OTHER(lm, enc_id)) {
		SDE_DEBUG("lm %d already reserved\n", lm_cfg->id);
		return false;
	}

	if (lm_cfg->dspp != DSPP_MAX) {
		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_DSPP);
		while (_sde_rm_get_hw_locked(rm, state, &iter)) {
			if (iter.blk->id == lm_cfg->dspp) {
				*dspp = iter.blk;
				break;
			}
		}

		if (!*dspp) {
			SDE_DEBUG("lm %d failed to retrieve dspp %d\n", lm->id,
					lm_cfg->dspp);
			return false;
		}

		if (RESERVED_BY_OTHER(*dspp, enc_id)) {
			SDE_DEBUG("lm %d dspp %d already reserved\n",
					lm->id, (*dspp)->id);
			return false;
		}
	}

	if (lm_cfg->ds != DS_MAX) {
		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_DS);
		while (_sde_rm_get_hw_locked(rm, state, &iter)) {
			if (iter.blk->id == lm_cfg->ds) {
				*ds = iter.blk;
				break;
			}
		}

		if (!*ds) {
			SDE_DEBUG("lm %d failed to retrieve ds %d\n", lm->id,
					lm_cfg->ds);
			return false;
		}

		if (RESERVED_BY_OTHER(*ds, enc_id)) {
			SDE_DEBUG("lm %d ds %d already reserved\n",
					lm->id, (*ds)->id);
			return false;
		}
	}

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_PINGPONG);
	while (_sde_rm_get_hw_locked(rm, state, &iter)) {
		if (iter.blk->id == lm_cfg->pingpong) {
			*pp = iter.blk;
			break;
		}
	}

	if (!*pp) {
		SDE_ERROR("failed to get pp on lm %d\n", lm_cfg->pingpong);
		return false;
	}

	if (RESERVED_BY_OTHER(*pp, enc_id)) {
		SDE_DEBUG("lm %d pp %d already reserved\n", lm->id,
				(*pp)->id);
		*dspp = NULL;
		*ds = NULL;
		return false;
	}

	if (lm_cfg->roi_misr != ROI_MISR_MAX) {
		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_ROI_MISR);
		while (_sde_rm_get_hw_locked(rm, state, &iter)) {
			if (iter.blk->id == lm_cfg->roi_misr) {
				*roi_misr = iter.blk;
				break;
			}
		}

		if (!*roi_misr) {
			SDE_ERROR("failed to get roi misr on lm %d\n",
					lm_cfg->roi_misr);
			return false;
		}

		if (RESERVED_BY_OTHER(*roi_misr, enc_id)) {
			SDE_DEBUG("lm %d roi_misr %d already reserved\n",
					lm->id, (*roi_misr)->id);
			*dspp = NULL;
			*ds = NULL;
			*pp = NULL;
			return false;
		}

		/**
		 * in 3DMux case, we should set the second roi misr to null,
		 * because it's not in the control path and only first roi
		 * misr is available.
		 */
		if (primary_lm
		    && sde_rm_is_3dmux_case(reqs->topology->top_name))
			*roi_misr = NULL;
	}

	/**
	 * if roi misr has been enabled in DT, DSC block
	 * should be reserved here and skip reserve DSC
	 * from free pool.
	 * Due to hardware limitation, DSC block should
	 * be reserved with roi misr id if both dsc and
	 * roi misr are enabled.
	 */
	if (reqs->topology->num_comp_enc
		&& sde_kms->catalog->has_roi_misr) {
		sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_DSC);
		while (_sde_rm_get_hw_locked(rm, state, &iter)) {
			if (iter.blk->id == lm_cfg->roi_misr) {
				*dsc = iter.blk;
				break;
			}
		}

		if (!*dsc) {
			SDE_ERROR("failed to get dsc on lm %d\n",
					lm_cfg->roi_misr);
			return false;
		}

		if (RESERVED_BY_OTHER(*dsc, enc_id)) {
			SDE_DEBUG("lm %d dsc %d already reserved\n",
					lm->id, (*dsc)->id);
			*dspp = NULL;
			*ds = NULL;
			*pp = NULL;
			*roi_misr = NULL;
			return false;
		}
	}

	pp_cfg = to_sde_hw_pingpong((*pp)->hw)->caps;
	if ((reqs->topology->top_name == SDE_RM_TOPOLOGY_PPSPLIT) &&
			!(test_bit(SDE_PINGPONG_SPLIT, &pp_cfg->features))) {
		SDE_DEBUG("pp %d doesn't support ppsplit\n", pp_cfg->id);
		*dspp = NULL;
		*ds = NULL;
		return false;
	}

	return true;
}

static int _sde_rm_reserve_lms(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		struct sde_rm_requirements *reqs,
		u8 *_lm_ids)

{
	struct sde_rm_hw_blk *lm[MAX_BLOCKS];
	struct sde_rm_hw_blk *dspp[MAX_BLOCKS];
	struct sde_rm_hw_blk *ds[MAX_BLOCKS];
	struct sde_rm_hw_blk *pp[MAX_BLOCKS];
	struct sde_rm_hw_blk *roi_misr[MAX_BLOCKS];
	struct sde_rm_hw_blk *dsc[MAX_BLOCKS];
	struct sde_rm_hw_iter iter_i, iter_j;
	u32 lm_mask = 0;
	int lm_count = 0;
	int i, rc = 0;
	uint64_t org_top_ctrl = reqs->top_ctrl;

	if (!reqs->topology->num_lm) {
		SDE_DEBUG("invalid number of lm: %d\n", reqs->topology->num_lm);
		return 0;
	}

	/* Find a primary mixer */
	sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_LM);
	while (lm_count != reqs->topology->num_lm) {
		if (!_sde_rm_get_hw_locked(rm, state, &iter_i)) {
			/*
			 * Prefer to give away low-resource mixers first:
			 * - Firstly check mixers without DSPPs and DSs
			 * - If failed, check mixers with DSPPs without DSs
			 * - If failed, check mixers with DSPPs and DSs
			 */
			if (!RM_RQ_DSPP(reqs)) {
				SDE_DEBUG("Try enable DSPP\n");
				reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DSPP);
				sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_LM);
				continue;
			} else if (!RM_RQ_DS(reqs)) {
				SDE_DEBUG("Try enable DS\n");
				reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DS);
				sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_LM);
				continue;
			}
			break;
		}

		if (lm_mask & (1 << iter_i.blk->id))
			continue;

		lm[lm_count] = iter_i.blk;
		dspp[lm_count] = NULL;
		ds[lm_count] = NULL;
		pp[lm_count] = NULL;
		roi_misr[lm_count] = NULL;
		dsc[lm_count] = NULL;

		SDE_DEBUG("blk id = %d, _lm_ids[%d] = %d\n",
			iter_i.blk->id,
			lm_count,
			_lm_ids ? _lm_ids[lm_count] : -1);

		if (_lm_ids && (lm[lm_count])->id != _lm_ids[lm_count])
			continue;

		if (!_sde_rm_check_lm_and_get_connected_blks(
				rm, state, enc_id, reqs, lm[lm_count],
				&dspp[lm_count], &ds[lm_count],
				&pp[lm_count], &roi_misr[lm_count],
				&dsc[lm_count], NULL))
			continue;

		lm_mask |= (1 << iter_i.blk->id);
		++lm_count;

		/* Return if peer is not needed */
		if (lm_count == reqs->topology->num_lm)
			break;

		/* Valid primary mixer found, find matching peers */
		sde_rm_init_hw_iter(&iter_j, 0, SDE_HW_BLK_LM);

		while (_sde_rm_get_hw_locked(rm, state, &iter_j)) {
			if (lm_mask & (1 << iter_j.blk->id))
				continue;

			lm[lm_count] = iter_j.blk;
			dspp[lm_count] = NULL;
			ds[lm_count] = NULL;
			pp[lm_count] = NULL;
			roi_misr[lm_count] = NULL;

			if (!_sde_rm_check_lm_and_get_connected_blks(
					rm, state, enc_id, reqs, iter_j.blk,
					&dspp[lm_count], &ds[lm_count],
					&pp[lm_count], &roi_misr[lm_count],
					&dsc[lm_count], iter_i.blk))
				continue;

			SDE_DEBUG("blk id = %d, _lm_ids[%d] = %d\n",
				iter_j.blk->id,
				lm_count,
				_lm_ids ? _lm_ids[lm_count] : -1);

			if (_lm_ids && (lm[lm_count])->id != _lm_ids[lm_count])
				continue;

			lm_mask |= (1 << iter_j.blk->id);
			++lm_count;
			break;
		}

		/* Rollback primary LM if peer is not found */
		if (!iter_j.hw) {
			lm_mask &= ~(1 << iter_i.blk->id);
			--lm_count;
		}
	}

	/* Restore the DSPP/DS request */
	reqs->top_ctrl = org_top_ctrl;

	if (lm_count != reqs->topology->num_lm) {
		SDE_DEBUG("unable to find appropriate mixers\n");
		return -ENAVAIL;
	}

	for (i = 0; i < lm_count; i++) {
		lm[i]->enc_id = enc_id;
		pp[i]->enc_id = enc_id;
		if (dspp[i])
			dspp[i]->enc_id = enc_id;

		if (ds[i])
			ds[i]->enc_id = enc_id;

		if (roi_misr[i])
			roi_misr[i]->enc_id = enc_id;

		if (dsc[i])
			dsc[i]->enc_id = enc_id;

		SDE_EVT32(lm[i]->type, enc_id, lm[i]->id, pp[i]->id,
				dspp[i] ? dspp[i]->id : 0,
				ds[i] ? ds[i]->id : 0,
				roi_misr[i] ? roi_misr[i]->id : 0,
				dsc[i] ? dsc[i]->id : 0);
	}

	if (reqs->topology->top_name == SDE_RM_TOPOLOGY_PPSPLIT) {
		/* reserve a free PINGPONG_SLAVE block */
		rc = -ENAVAIL;
		sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_PINGPONG);
		while (_sde_rm_get_hw_locked(rm, state, &iter_i)) {
			const struct sde_hw_pingpong *pp =
					to_sde_hw_pingpong(iter_i.blk->hw);
			const struct sde_pingpong_cfg *pp_cfg = pp->caps;

			if (!(test_bit(SDE_PINGPONG_SLAVE, &pp_cfg->features)))
				continue;
			if (RESERVED_BY_OTHER(iter_i.blk, enc_id))
				continue;

			iter_i.blk->enc_id = enc_id;
			rc = 0;
			break;
		}
	}

	return rc;
}

static int _sde_rm_reserve_ctls(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		struct sde_rm_requirements *reqs,
		const struct sde_rm_topology_def *top,
		u8 *_ctl_ids)
{
	struct sde_rm_hw_blk *ctls[MAX_BLOCKS];
	struct sde_rm_hw_iter iter;
	int i = 0;

	if (!top->num_ctl) {
		SDE_DEBUG("invalid number of ctl: %d\n", top->num_ctl);
		return 0;
	}

	memset(&ctls, 0, sizeof(ctls));

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_CTL);
	while (_sde_rm_get_hw_locked(rm, state, &iter)) {
		const struct sde_hw_ctl *ctl = to_sde_hw_ctl(iter.blk->hw);
		unsigned long features = ctl->caps->features;
		bool has_split_display, has_ppsplit, primary_pref;

		if (RESERVED_BY_OTHER(iter.blk, enc_id))
			continue;

		has_split_display = BIT(SDE_CTL_SPLIT_DISPLAY) & features;
		has_ppsplit = BIT(SDE_CTL_PINGPONG_SPLIT) & features;
		primary_pref = BIT(SDE_CTL_PRIMARY_PREF) & features;

		SDE_DEBUG("ctl %d caps 0x%lX\n", iter.blk->id, features);

		/*
		 * bypass rest feature checks on finding CTL preferred
		 * for primary displays.
		 */
		if (!primary_pref && !_ctl_ids) {
			if (top->needs_split_display != has_split_display)
				continue;

			if (top->top_name == SDE_RM_TOPOLOGY_PPSPLIT &&
					!has_ppsplit)
				continue;
		} else if (!(reqs->hw_res.is_primary && primary_pref) &&
				!_ctl_ids) {
			SDE_DEBUG(
				"display pref not met. is_primary: %d primary_pref: %d\n",
				reqs->hw_res.is_primary, primary_pref);
			continue;
		}

		ctls[i] = iter.blk;

		SDE_DEBUG("blk id = %d, _ctl_ids[%d] = %d\n",
			iter.blk->id, i,
			_ctl_ids ? _ctl_ids[i] : -1);

		if (_ctl_ids && (ctls[i]->id != _ctl_ids[i]))
			continue;

		SDE_DEBUG("ctl %d match\n", iter.blk->id);

		if (++i == top->num_ctl)
			break;
	}

	if (i != top->num_ctl)
		return -ENAVAIL;

	for (i = 0; i < ARRAY_SIZE(ctls) && i < top->num_ctl; i++) {
		ctls[i]->enc_id = enc_id;
		SDE_EVT32(ctls[i]->type, enc_id, ctls[i]->id);
	}

	return 0;
}


static bool _sde_rm_check_dsc(struct sde_rm *rm,
		uint32_t enc_id,
		struct sde_rm_hw_blk *dsc,
		struct sde_rm_hw_blk *paired_dsc)
{
	const struct sde_dsc_cfg *dsc_cfg = to_sde_hw_dsc(dsc->hw)->caps;

	/* Already reserved? */
	if (RESERVED_BY_OTHER(dsc, enc_id)) {
		SDE_DEBUG("dsc %d already reserved\n", dsc_cfg->id);
		return false;
	}

	/* Check if this dsc is a peer of the proposed paired DSC */
	if (paired_dsc) {
		const struct sde_dsc_cfg *paired_dsc_cfg =
				to_sde_hw_dsc(paired_dsc->hw)->caps;

		if (!test_bit(dsc_cfg->id, &paired_dsc_cfg->dsc_pair_mask)) {
			SDE_DEBUG("dsc %d not peer of dsc %d\n", dsc_cfg->id,
					paired_dsc_cfg->id);
			return false;
		}
	}

	return true;
}

static int _sde_rm_reserve_dsc(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		const struct sde_rm_topology_def *top,
		u8 *_dsc_ids)
{
	struct sde_rm_hw_iter iter_i, iter_j;
	struct sde_rm_hw_blk *dsc[MAX_BLOCKS];
	u32 reserve_mask = 0;
	struct msm_drm_private *priv = rm->dev->dev_private;
	struct sde_kms *sde_kms;
	int alloc_count = 0;
	int num_dsc_enc = top->num_comp_enc;
	int i;

	if (!top->num_comp_enc)
		return 0;

	sde_kms = to_sde_kms(priv->kms);

	/**
	 * if roi misr has been enabled in DT, DSC block
	 * should be reserved from lm reservation function
	 * and skip here.
	 */
	if (sde_kms->catalog->has_roi_misr)
		return 0;

	sde_rm_init_hw_iter(&iter_i, 0, SDE_HW_BLK_DSC);

	while (alloc_count != num_dsc_enc &&
			_sde_rm_get_hw_locked(rm, state, &iter_i)) {
		if (reserve_mask & (1 << iter_i.blk->id))
			continue;

		if (_dsc_ids && (iter_i.blk->id != _dsc_ids[alloc_count]))
			continue;

		if (!_sde_rm_check_dsc(rm, enc_id, iter_i.blk, NULL))
			continue;

		SDE_DEBUG("blk id = %d, _dsc_ids[%d] = %d\n",
			iter_i.blk->id,
			alloc_count,
			_dsc_ids ? _dsc_ids[alloc_count] : -1);

		reserve_mask |= 1 << iter_i.blk->id;
		dsc[alloc_count++] = iter_i.blk;

		/* Return if peer is not needed */
		if (alloc_count == num_dsc_enc)
			break;

		/* Valid first dsc found, find matching peers */
		sde_rm_init_hw_iter(&iter_j, 0, SDE_HW_BLK_DSC);

		while (_sde_rm_get_hw_locked(rm, state, &iter_j)) {
			if (reserve_mask & (1 << iter_j.blk->id))
				continue;

			if (_dsc_ids && (iter_j.blk->id !=
					_dsc_ids[alloc_count]))
				continue;

			if (!_sde_rm_check_dsc(rm, enc_id, iter_j.blk,
					iter_i.blk))
				continue;

			SDE_DEBUG("blk id = %d, _dsc_ids[%d] = %d\n",
				iter_j.blk->id,
				alloc_count,
				_dsc_ids ? _dsc_ids[alloc_count] : -1);

			reserve_mask |= 1 << iter_j.blk->id;
			dsc[alloc_count++] = iter_j.blk;
			break;
		}

		/* Rollback primary DSC if peer is not found */
		if (!iter_j.hw) {
			reserve_mask &= ~(1 << iter_i.blk->id);
			--alloc_count;
		}
	}

	if (alloc_count != num_dsc_enc) {
		SDE_ERROR("couldn't reserve %d dsc blocks for enc id %d\n",
			num_dsc_enc, enc_id);
		return -ENAVAIL;
	}

	for (i = 0; i < alloc_count; i++) {
		if (!dsc[i])
			break;

		dsc[i]->enc_id = enc_id;

		SDE_EVT32(dsc[i]->type, enc_id, dsc[i]->id);
	}

	return 0;
}

static int _sde_rm_reserve_qdss(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		const struct sde_rm_topology_def *top,
		u8 *_qdss_ids)
{
	struct sde_rm_hw_iter iter;
	struct msm_drm_private *priv = rm->dev->dev_private;
	struct sde_kms *sde_kms;

	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_QDSS);

	while (_sde_rm_get_hw_locked(rm, state, &iter)) {
		if (RESERVED_BY_OTHER(iter.blk, enc_id))
			continue;

		SDE_DEBUG("blk id = %d\n", iter.blk->id);

		iter.blk->enc_id = enc_id;
		SDE_EVT32(iter.blk->type, enc_id, iter.blk->id);
		return 0;
	}

	if (!iter.hw && sde_kms->catalog->qdss_count) {
		SDE_DEBUG("couldn't reserve qdss for type %d id %d\n",
						SDE_HW_BLK_QDSS, iter.blk->id);
		return -ENAVAIL;
	}

	return 0;
}

static int _sde_rm_reserve_cdm(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		uint32_t id,
		enum sde_hw_blk_type type)
{
	struct sde_rm_hw_iter iter;

	sde_rm_init_hw_iter(&iter, 0, SDE_HW_BLK_CDM);
	while (_sde_rm_get_hw_locked(rm, state, &iter)) {
		const struct sde_hw_cdm *cdm = to_sde_hw_cdm(iter.blk->hw);
		const struct sde_cdm_cfg *caps = cdm->caps;
		bool match = false;

		if (RESERVED_BY_OTHER(iter.blk, enc_id))
			continue;

		if (type == SDE_HW_BLK_INTF && id != INTF_MAX)
			match = test_bit(id, &caps->intf_connect);
		else if (type == SDE_HW_BLK_WB && id != WB_MAX)
			match = test_bit(id, &caps->wb_connect);

		SDE_DEBUG("type %d id %d, cdm intfs %lu wbs %lu match %d\n",
				type, id, caps->intf_connect, caps->wb_connect,
				match);

		if (!match)
			continue;

		iter.blk->enc_id = enc_id;
		SDE_EVT32(iter.blk->type, enc_id, iter.blk->id);
		break;
	}

	if (!iter.hw) {
		SDE_ERROR("couldn't reserve cdm for type %d id %d\n", type, id);
		return -ENAVAIL;
	}

	return 0;
}

static int _sde_rm_reserve_intf_or_wb(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		uint32_t id,
		enum sde_hw_blk_type type,
		bool needs_cdm)
{
	struct sde_rm_hw_iter iter;
	int ret = 0;

	/* Find the block entry in the rm, and note the reservation */
	sde_rm_init_hw_iter(&iter, 0, type);
	while (_sde_rm_get_hw_locked(rm, state, &iter)) {
		if (iter.blk->id != id)
			continue;

		if (RESERVED_BY_OTHER(iter.blk, enc_id)) {
			SDE_ERROR("type %d id %d already reserved\n", type, id);
			return -ENAVAIL;
		}

		iter.blk->enc_id = enc_id;
		SDE_EVT32(iter.blk->type, enc_id, iter.blk->id);
		break;
	}

	/* Shouldn't happen since wbs / intfs are fixed at probe */
	if (!iter.hw) {
		SDE_ERROR("couldn't find type %d id %d\n", type, id);
		return -EINVAL;
	}

	/* Expected only one intf or wb will request cdm */
	if (needs_cdm)
		ret = _sde_rm_reserve_cdm(rm, state, enc_id, id, type);

	return ret;
}

static int _sde_rm_reserve_intf_related_hw(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id,
		struct sde_encoder_hw_resources *hw_res)
{
	int i, ret = 0;
	u32 id;

	for (i = 0; i < ARRAY_SIZE(hw_res->intfs); i++) {
		if (hw_res->intfs[i] == INTF_MODE_NONE)
			continue;
		id = i + INTF_0;
		ret = _sde_rm_reserve_intf_or_wb(rm, state, enc_id, id,
				SDE_HW_BLK_INTF, hw_res->needs_cdm);
		if (ret)
			return ret;
	}

	for (i = 0; i < ARRAY_SIZE(hw_res->wbs); i++) {
		if (hw_res->wbs[i] == INTF_MODE_NONE)
			continue;
		id = i + WB_0;
		ret = _sde_rm_reserve_intf_or_wb(rm, state, enc_id, id,
				SDE_HW_BLK_WB, hw_res->needs_cdm);
		if (ret)
			return ret;
	}

	return ret;
}

static int _sde_rm_make_next_rsvp(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		struct sde_rm_requirements *reqs)
{
	int ret;
	uint32_t enc_id = enc->base.id;
	struct sde_rm_topology_def topology;

	/*
	 * Assign LMs and blocks whose usage is tied to them: DSPP & Pingpong.
	 */
	ret = _sde_rm_reserve_lms(rm, state, enc_id, reqs, NULL);
	if (ret) {
		SDE_ERROR("unable to find appropriate mixers\n");
		return ret;
	}

	/*
	 * Do assignment preferring to give away low-resource CTLs first:
	 * - Check mixers without Split Display
	 * - Only then allow to grab from CTLs with split display capability
	 */
	_sde_rm_reserve_ctls(rm, state, enc_id, reqs, reqs->topology, NULL);
	if (ret && !reqs->topology->needs_split_display &&
			reqs->topology->num_ctl > SINGLE_CTL) {
		memcpy(&topology, reqs->topology, sizeof(topology));
		topology.needs_split_display = true;
		_sde_rm_reserve_ctls(rm, state, enc_id, reqs, &topology, NULL);
	}
	if (ret) {
		SDE_ERROR("unable to find appropriate CTL\n");
		return ret;
	}

	/* Assign INTFs, WBs, and blks whose usage is tied to them: CTL & CDM */
	ret = _sde_rm_reserve_intf_related_hw(rm, state, enc_id,
			&reqs->hw_res);
	if (ret)
		return ret;

	ret = _sde_rm_reserve_dsc(rm, state, enc_id, reqs->topology, NULL);
	if (ret)
		return ret;

	ret = _sde_rm_reserve_qdss(rm, state, enc_id, reqs->topology, NULL);
	if (ret)
		return ret;

	return ret;
}

/**
 * _sde_rm_get_hw_blk_for_cont_splash - retrieve the LM blocks on given CTL
 * and populate the connected HW blk ids in sde_splash_display
 * @rm:	Pointer to resource manager structure
 * @state: Pointer to resource manager state structure
 * @ctl: Pointer to CTL hardware block
 * @splash_display: Pointer to struct sde_splash_display
 * return: number of active LM blocks for this CTL block
 */
static int _sde_rm_get_hw_blk_for_cont_splash(struct sde_rm *rm,
		struct sde_rm_state *state,
		struct sde_hw_ctl *ctl,
		struct sde_splash_display *splash_display)
{
	u32 lm_reg;
	struct sde_rm_hw_iter iter_lm, iter_pp;
	struct sde_hw_pingpong *pp;

	if (!rm || !ctl || !splash_display) {
		SDE_ERROR("invalid input parameters\n");
		return 0;
	}

	sde_rm_init_hw_iter(&iter_lm, 0, SDE_HW_BLK_LM);
	sde_rm_init_hw_iter(&iter_pp, 0, SDE_HW_BLK_PINGPONG);
	while (_sde_rm_get_hw_locked(rm, state, &iter_lm)) {
		_sde_rm_get_hw_locked(rm, state, &iter_pp);

		if (splash_display->lm_cnt >= MAX_DATA_PATH_PER_DSIPLAY)
			break;

		lm_reg = ctl->ops.read_ctl_layers(ctl, iter_lm.blk->id);
		if (!lm_reg)
			continue;

		splash_display->lm_ids[splash_display->lm_cnt++] =
			iter_lm.blk->id;
		SDE_DEBUG("lm_cnt=%d lm_reg[%d]=0x%x\n", splash_display->lm_cnt,
				iter_lm.blk->id - LM_0, lm_reg);

		if (ctl->ops.get_staged_sspp &&
				ctl->ops.get_staged_sspp(ctl, iter_lm.blk->id,
					&splash_display->pipes[
					splash_display->pipe_cnt], 1)) {
			splash_display->pipe_cnt++;
		} else {
			SDE_ERROR("no pipe detected on LM-%d\n",
					iter_lm.blk->id - LM_0);
			return 0;
		}

		pp = to_sde_hw_pingpong(iter_pp.blk->hw);
		if (pp && pp->ops.get_dsc_status &&
				pp->ops.get_dsc_status(pp)) {
			splash_display->dsc_ids[splash_display->dsc_cnt++] =
				iter_pp.blk->id;
			SDE_DEBUG("lm/pp[%d] path, using dsc[%d]\n",
					iter_lm.blk->id - LM_0,
					iter_pp.blk->id - DSC_0);
		}
	}

	return splash_display->lm_cnt;
}

int sde_rm_cont_splash_res_init(struct msm_drm_private *priv,
				struct sde_rm *rm,
				struct sde_splash_data *splash_data,
				struct sde_mdss_cfg *cat)
{
	struct sde_rm_hw_iter iter_c;
	struct sde_rm_state *state;
	int index = 0, ctl_top_cnt;
	struct sde_kms *sde_kms = NULL;
	struct sde_hw_mdp *hw_mdp;
	struct sde_splash_display *splash_display;
	u8 intf_sel;

	if (!priv || !rm || !cat || !splash_data) {
		SDE_ERROR("invalid input parameters\n");
		return -EINVAL;
	}

	SDE_DEBUG("mixer_count=%d, ctl_count=%d, dsc_count=%d\n",
			cat->mixer_count,
			cat->ctl_count,
			cat->dsc_count);

	ctl_top_cnt = cat->ctl_count;

	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	hw_mdp = sde_rm_get_mdp(rm);

	state = to_sde_rm_priv_state(rm->obj.state);

	sde_rm_init_hw_iter(&iter_c, 0, SDE_HW_BLK_CTL);
	while (_sde_rm_get_hw_locked(rm, state, &iter_c)) {
		struct sde_hw_ctl *ctl = to_sde_hw_ctl(iter_c.blk->hw);

		if (!ctl->ops.get_ctl_intf) {
			SDE_ERROR("get_ctl_intf not initialized\n");
			return -EINVAL;
		}

		intf_sel = ctl->ops.get_ctl_intf(ctl);
		if (intf_sel) {
			splash_display =  &splash_data->splash_display[index];
			SDE_DEBUG("finding resources for display=%d ctl=%d\n",
					index, iter_c.blk->id - CTL_0);

			_sde_rm_get_hw_blk_for_cont_splash(rm, state,
					ctl, splash_display);
			splash_display->cont_splash_enabled = true;
			splash_display->ctl_ids[splash_display->ctl_cnt++] =
				iter_c.blk->id;

			if (hw_mdp && hw_mdp->ops.get_split_flush_status) {
				splash_display->single_flush_en =
					hw_mdp->ops.get_split_flush_status(
							hw_mdp);
			}

			if (!splash_display->single_flush_en ||
					(iter_c.blk->id != CTL_0))
				index++;

			if (index >= ARRAY_SIZE(splash_data->splash_display))
				break;
		}
	}

	if (index != splash_data->num_splash_displays) {
		SDE_DEBUG("mismatch active displays vs actually enabled :%d/%d",
				splash_data->num_splash_displays, index);
		return -EINVAL;
	}

	return 0;
}

static int _sde_rm_make_next_rsvp_for_cont_splash(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		struct sde_rm_requirements *reqs)
{
	int ret;
	uint32_t enc_id = enc->base.id;
	struct sde_rm_topology_def topology;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_splash_display *splash_display = NULL;
	int i;

	if (!enc->dev || !enc->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return -EINVAL;
	}
	priv = enc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	for (i = 0; i < ARRAY_SIZE(sde_kms->splash_data.splash_display); i++) {
		if (enc == sde_kms->splash_data.splash_display[i].encoder)
			splash_display =
				&sde_kms->splash_data.splash_display[i];
	}

	if (!splash_display) {
		SDE_ERROR("invalid splash data for enc:%d\n", enc->base.id);
		return -EINVAL;
	}

	for (i = 0; i < splash_display->lm_cnt; i++)
		SDE_DEBUG("splash_data.lm_ids[%d] = %d\n",
			i, splash_display->lm_ids[i]);

	if (splash_display->lm_cnt !=
			reqs->topology->num_lm)
		SDE_DEBUG("Configured splash screen LMs != needed LM cnt\n");

	/*
	 * Assign LMs and blocks whose usage is tied to them: DSPP & Pingpong.
	 * Do assignment preferring to give away low-resource mixers first:
	 * - Check mixers without DSPPs
	 * - Only then allow to grab from mixers with DSPP capability
	 */
	ret = _sde_rm_reserve_lms(rm, state, enc_id, reqs,
				splash_display->lm_ids);
	if (ret && !RM_RQ_DSPP(reqs)) {
		reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DSPP);
		ret = _sde_rm_reserve_lms(rm, state, enc_id, reqs,
					splash_display->lm_ids);
	}

	if (ret) {
		SDE_ERROR("unable to find appropriate mixers\n");
		return ret;
	}

	/*
	 * Do assignment preferring to give away low-resource CTLs first:
	 * - Check mixers without Split Display
	 * - Only then allow to grab from CTLs with split display capability
	 */
	for (i = 0; i < splash_display->ctl_cnt; i++)
		SDE_DEBUG("splash_data.ctl_ids[%d] = %d\n",
			i, splash_display->ctl_ids[i]);

	_sde_rm_reserve_ctls(rm, state, enc_id, reqs, reqs->topology,
			splash_display->ctl_ids);
	if (ret && !reqs->topology->needs_split_display) {
		memcpy(&topology, reqs->topology, sizeof(topology));
		topology.needs_split_display = true;
		_sde_rm_reserve_ctls(rm, state, enc_id, reqs, &topology,
				splash_display->ctl_ids);
	}
	if (ret) {
		SDE_ERROR("unable to find appropriate CTL\n");
		return ret;
	}

	/* Assign INTFs, WBs, and blks whose usage is tied to them: CTL & CDM */
	ret = _sde_rm_reserve_intf_related_hw(rm, state, enc_id,
			&reqs->hw_res);
	if (ret)
		return ret;

	for (i = 0; i < splash_display->dsc_cnt; i++)
		SDE_DEBUG("splash_data.dsc_ids[%d] = %d\n",
			i, splash_display->dsc_ids[i]);

	ret = _sde_rm_reserve_dsc(rm, state, enc_id, reqs->topology,
				splash_display->dsc_ids);
	if (ret)
		return ret;

	ret = _sde_rm_reserve_qdss(rm, state, enc_id, reqs->topology, NULL);
	if (ret)
		return ret;

	return ret;
}

static int _sde_rm_populate_requirements(
		struct sde_rm *rm,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state,
		struct sde_rm_requirements *reqs)
{
	const struct drm_display_mode *mode = &crtc_state->mode;
	int i, num_lm;

	memset(reqs, 0, sizeof(*reqs));

	reqs->top_ctrl = sde_connector_get_property(conn_state,
			CONNECTOR_PROP_TOPOLOGY_CONTROL);
	sde_encoder_get_hw_resources(enc, &reqs->hw_res, conn_state);

	for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++) {
		if (RM_IS_TOPOLOGY_MATCH(rm->topology_tbl[i],
					reqs->hw_res.topology)) {
			reqs->topology = &rm->topology_tbl[i];
			break;
		}
	}

	if (!reqs->topology) {
		SDE_ERROR("invalid topology for the display\n");
		return -EINVAL;
	}

	/*
	 * select dspp HW block for all dsi displays and ds for only
	 * primary dsi display.
	 */
	if (conn_state->connector->connector_type == DRM_MODE_CONNECTOR_DSI) {
		if (!RM_RQ_DSPP(reqs))
			reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DSPP);

		if (!RM_RQ_DS(reqs) && rm->hw_mdp->caps->has_dest_scaler &&
		    sde_encoder_is_primary_display(enc))
			reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_DS);
	}

	/**
	 * Set the requirement for LM which has CWB support if CWB is
	 * found enabled.
	 */
	if (!RM_RQ_CWB(reqs) && sde_encoder_in_clone_mode(enc)) {
		reqs->top_ctrl |= BIT(SDE_RM_TOPCTL_CWB);

		/*
		 * topology selection based on conn mode is not valid for CWB
		 * as WB conn populates modes based on max_mixer_width check
		 * but primary can be using dual LMs. This topology override for
		 * CWB is to check number of datapath active in primary and
		 * allocate same number of LM/PP blocks reserved for CWB
		 */
		reqs->topology =
			&rm->topology_tbl[SDE_RM_TOPOLOGY_DUALPIPE_3DMERGE];

		num_lm = sde_crtc_get_num_datapath(crtc_state->crtc,
				conn_state->connector);

		if (num_lm == 1)
			reqs->topology =
				&rm->topology_tbl[SDE_RM_TOPOLOGY_SINGLEPIPE];
		else if (num_lm == 0)
			SDE_ERROR("Primary layer mixer is not set\n");

		SDE_EVT32(num_lm, reqs->topology->num_lm,
			reqs->topology->top_name, reqs->topology->num_ctl);
	}

	SDE_DEBUG("top_ctrl: 0x%llX num_h_tiles: %d\n", reqs->top_ctrl,
			reqs->hw_res.display_num_of_h_tiles);
	SDE_DEBUG("num_lm: %d num_ctl: %d topology: %d split_display: %d\n",
			reqs->topology->num_lm, reqs->topology->num_ctl,
			reqs->topology->top_name,
			reqs->topology->needs_split_display);
	SDE_EVT32(mode->hdisplay, rm->lm_max_width, reqs->topology->num_lm,
			reqs->top_ctrl, reqs->topology->top_name,
			reqs->topology->num_ctl);

	return 0;
}

int sde_rm_update_topology(struct sde_rm *rm,
	struct drm_connector_state *conn_state,
	struct msm_display_topology *topology)
{
	int i, ret = 0;
	struct msm_display_topology top;
	enum sde_rm_topology_name top_name = SDE_RM_TOPOLOGY_NONE;

	if (!conn_state)
		return -EINVAL;

	if (topology) {
		top = *topology;
		for (i = 0; i < SDE_RM_TOPOLOGY_MAX; i++)
			if (RM_IS_TOPOLOGY_MATCH(rm->topology_tbl[i], top)) {
				top_name = rm->topology_tbl[i].top_name;
				break;
			}
	}

	ret = msm_property_set_property(
			sde_connector_get_propinfo(conn_state->connector),
			sde_connector_get_property_state(conn_state),
			CONNECTOR_PROP_TOPOLOGY_NAME, top_name);

	return ret;
}

/**
 * _sde_rm_release_rsvp - release resources and release a reservation
 * @rm:	KMS handle
 * @rsvp:	RSVP pointer to release and release resources for
 */
static void _sde_rm_release_rsvp(
		struct sde_rm *rm,
		struct sde_rm_state *state,
		uint32_t enc_id)
{
	struct sde_rm_hw_blk *blk, *p;
	enum sde_hw_blk_type type;

	SDE_DEBUG("rel enc %d\n", enc_id);

	for (type = 0; type < SDE_HW_BLK_MAX; type++) {
		list_for_each_entry_safe(blk, p, &state->hw_blks[type], list) {
			if (blk->enc_id == enc_id) {
				/*
				 * external block is created at reserve time
				 * and destroyed at release time.
				 */
				if (blk->ext_hw) {
					SDE_DEBUG("remove enc %d %d %d\n",
							enc_id,
							blk->type, blk->id);
					list_del(&blk->list);
					kfree(blk);
					continue;
				}

				blk->enc_id = 0;
				SDE_DEBUG("rel enc %d %d %d\n",
						enc_id,
						blk->type, blk->id);
			}
		}
	}
}

int sde_rm_release(struct sde_rm *rm,
		struct drm_encoder *enc,
		struct drm_atomic_state *atomic_state)
{
	struct sde_rm_state *state;

	if (!rm || !enc || !atomic_state) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	state = sde_rm_get_atomic_state(atomic_state, rm);
	if (IS_ERR(state))
		return PTR_ERR(state);

	_sde_rm_print_rsvps(state, SDE_RM_STAGE_BEGIN);

	_sde_rm_release_rsvp(rm, state, enc->base.id);

	_sde_rm_print_rsvps(state, SDE_RM_STAGE_AFTER_CLEAR);

	return 0;
}

int sde_rm_reserve(
		struct sde_rm *rm,
		struct drm_encoder *enc,
		struct drm_crtc_state *crtc_state,
		struct drm_connector_state *conn_state)
{
	struct sde_rm_state *state;
	struct sde_rm_requirements reqs;
	struct msm_drm_private *priv;
	struct sde_kms *sde_kms;
	struct sde_connector *sde_conn;
	int ret;

	if (!rm || !enc || !crtc_state || !conn_state) {
		SDE_ERROR("invalid arguments\n");
		return -EINVAL;
	}

	if (!enc->dev || !enc->dev->dev_private) {
		SDE_ERROR("drm device invalid\n");
		return -EINVAL;
	}

	/* shared connector doesn't have resources */
	sde_conn = to_sde_connector(conn_state->connector);
	if (sde_conn->shared)
		return 0;

	if (conn_state->state)
		state = sde_rm_get_atomic_state(conn_state->state, rm);
	else
		state = to_sde_rm_priv_state(rm->obj.state);

	if (IS_ERR(state))
		return PTR_ERR(state);

	priv = enc->dev->dev_private;
	if (!priv->kms) {
		SDE_ERROR("invalid kms\n");
		return -EINVAL;
	}
	sde_kms = to_sde_kms(priv->kms);

	SDE_DEBUG("reserving hw for conn %d enc %d crtc %d\n",
			conn_state->connector->base.id, enc->base.id,
			crtc_state->crtc->base.id);
	SDE_EVT32(enc->base.id, conn_state->connector->base.id);

	_sde_rm_print_rsvps(state, SDE_RM_STAGE_BEGIN);

	ret = _sde_rm_populate_requirements(rm, enc, crtc_state,
			conn_state, &reqs);
	if (ret) {
		SDE_ERROR("failed to populate hw requirements\n");
		goto end;
	}

	if (!conn_state->state) {
		SDE_DEBUG("cont_splash enabled on enc-%d\n", enc->base.id);
		ret = _sde_rm_make_next_rsvp_for_cont_splash(rm, state,
			enc, crtc_state, conn_state, &reqs);
	} else {
		ret = _sde_rm_make_next_rsvp(rm, state,
			enc, crtc_state, conn_state, &reqs);
	}

	_sde_rm_print_rsvps(state, SDE_RM_STAGE_AFTER_RSVPNEXT);

end:
	return ret;
}

int sde_rm_ext_blk_create_reserve(struct sde_rm *rm,
		struct drm_atomic_state *atomic_state,
		struct sde_hw_blk *hw, struct drm_encoder *enc)
{
	struct sde_rm_state *state;
	struct sde_rm_hw_blk *blk;
	int ret = 0;

	if (!rm || !hw || !enc) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	if (hw->type >= SDE_HW_BLK_MAX) {
		SDE_ERROR("invalid HW type\n");
		return -EINVAL;
	}

	state = sde_rm_get_atomic_state(atomic_state, rm);
	if (IS_ERR(state))
		return PTR_ERR(state);

	blk = kzalloc(sizeof(*blk), GFP_KERNEL);
	if (!blk) {
		ret = -ENOMEM;
		goto end;
	}

	blk->type = hw->type;
	blk->id = hw->id;
	blk->hw = hw;
	blk->enc_id = enc->base.id;
	blk->ext_hw = true;
	list_add_tail(&blk->list, &state->hw_blks[hw->type]);

	SDE_DEBUG("create blk %d %d for enc %d\n", blk->type, blk->id,
			enc->base.id);

end:
	return ret;
}
