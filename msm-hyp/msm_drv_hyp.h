/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * Copyright (c) 2017-2018, 2020-2021 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MSM_DRV_HYP_H__
#define __MSM_DRV_HYP_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/dma-buf.h>
#include <linux/atomic.h>
#include <linux/kthread.h>
#include <linux/component.h>
#include <linux/backlight.h>
#include <drm/drmP.h>
#include <drm/drm_gem.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_connector.h>
#include <drm/drm_client.h>
#include <drm/msm_drm.h>
#include <drm/sde_drm.h>
#include "msm_hyp_fence.h"
#include "msm_drv.h"

struct msm_hyp_connector_info {
	int connector_type;
	const struct drm_bridge_funcs *bridge_funcs;
	const struct drm_connector_helper_funcs *connector_funcs;
	uint32_t possible_crtcs;
	struct drm_display_info display_info;
	const char *display_type;
	const char *extra_caps;
};

struct msm_hyp_plane_info {
	enum drm_plane_type plane_type;
	const struct drm_plane_helper_funcs *plane_funcs;
	uint32_t possible_crtcs;
	uint32_t format_count;
	uint32_t *format_types;
	uint32_t maxdwnscale;
	uint32_t maxupscale;
	uint32_t maxhdeciexp;
	uint32_t maxvdeciexp;
	uint32_t max_width;
	uint64_t max_bandwidth;
	bool support_scale;
	bool support_csc;
	bool support_multirect;
	int master_plane_index;
	const char *extra_caps;
};

struct msm_hyp_crtc_info {
	const struct drm_crtc_helper_funcs *crtc_funcs;
	uint32_t primary_plane_index;
	uint32_t max_blendstages;
	uint64_t max_mdp_clk;
	uint64_t max_bandwidth_low;
	uint64_t max_bandwidth_high;
	const char *qseed_type;
	const char *smart_dma_rev;
	bool has_src_split;
	bool has_hdr;
	const char *extra_caps;
};

struct msm_hyp_framebuffer_info {
	void (*destroy)(struct drm_framebuffer *framebuffer);
};

struct msm_hyp_connector {
	struct drm_connector base;
	struct drm_encoder encoder;
	struct drm_bridge bridge;
	struct msm_hyp_fence_context *retire_fence;
	struct backlight_device *bl_device;
	struct msm_hyp_connector_info *info;
	struct drm_property_blob *blob_caps;
	struct drm_property_blob *blob_mode_info;
	struct drm_property_blob *blob_edid;
};

struct msm_hyp_connector_state {
	struct drm_connector_state base;
	uint64_t __user *retire_fence_ptr;
};

struct msm_hyp_plane {
	struct drm_plane base;
	struct drm_plane *primary_plane;
	struct msm_hyp_plane_info *info;
	struct drm_property_blob *blob_caps;
};

struct msm_hyp_plane_state {
	struct drm_plane_state base;
	struct dma_fence *input_fence;
	uint32_t zpos;
	uint32_t blend_op;
	uint32_t alpha;
	uint32_t fb_mode;
	uint32_t multirect_mode;
	struct sde_drm_csc_v1 csc;
	struct sde_drm_scaler_v2 scaler;
};

struct msm_hyp_crtc {
	struct drm_crtc base;
	struct msm_hyp_fence_context *output_fence;
	struct msm_hyp_crtc_info *info;
	struct task_struct *thread;
	struct kthread_worker worker;
	struct completion commit_done;
	struct drm_property_blob *blob_caps;
};

struct msm_hyp_crtc_state {
	struct drm_crtc_state base;
	uint32_t input_fence_timeout;
	uint32_t output_fence_offset;
	uint64_t __user *output_fence_ptr;
};

struct msm_hyp_framebuffer {
	struct drm_framebuffer base;
	struct drm_gem_object *bo;
	struct msm_hyp_framebuffer_info *info;
};

struct msm_hyp_mode_info {
	uint32_t num_lm;
	uint32_t num_enc;
	uint32_t num_intf;
};

#define to_msm_hyp_connector(x)\
		container_of((x), struct msm_hyp_connector, base)

#define to_msm_hyp_connector_state(x)\
		container_of((x), struct msm_hyp_connector_state, base)

#define to_msm_hyp_plane(x)\
		container_of((x), struct msm_hyp_plane, base)

#define to_msm_hyp_plane_state(x)\
		container_of((x), struct msm_hyp_plane_state, base)

#define to_msm_hyp_crtc(x)\
		container_of((x), struct msm_hyp_crtc, base)

#define to_msm_hyp_crtc_state(x)\
		container_of((x), struct msm_hyp_crtc_state, base)

#define to_msm_hyp_fb(x)\
		container_of((x), struct msm_hyp_framebuffer, base)

struct msm_hyp_kms;

struct msm_hyp_kms_funcs {
	int (*get_connector_infos)(struct msm_hyp_kms *kms,
			struct msm_hyp_connector_info **connector_infos,
			int *connector_num);
	int (*get_plane_infos)(struct msm_hyp_kms *kms,
			struct msm_hyp_plane_info **plane_infos,
			int *plane_num);
	int (*get_crtc_infos)(struct msm_hyp_kms *kms,
			struct msm_hyp_crtc_info **crtc_infos,
			int *crtc_num);
	int (*get_mode_info)(struct msm_hyp_kms *kms,
			const struct drm_display_mode *mode,
			struct msm_hyp_mode_info *modeinfo);
	int (*get_framebuffer_info)(struct msm_hyp_kms *kms,
			struct drm_framebuffer *fb,
			struct msm_hyp_framebuffer_info **fb_info);

	void (*prepare_commit)(struct msm_hyp_kms *kms,
			struct drm_atomic_state *old_state);
	void (*commit)(struct msm_hyp_kms *kms,
			struct drm_atomic_state *old_state);
	void (*complete_commit)(struct msm_hyp_kms *kms,
			struct drm_atomic_state *old_state);

	void (*enable_vblank)(struct msm_hyp_kms *kms,
			struct drm_crtc *crtc);
	void (*disable_vblank)(struct msm_hyp_kms *kms,
			struct drm_crtc *crtc);
};

struct msm_hyp_kms {
	const struct msm_hyp_kms_funcs *funcs;
};

struct msm_hyp_drm_private {
	struct drm_device *dev;
	struct msm_hyp_kms *kms;
	struct drm_driver driver;

	struct drm_property *prop_retire_fence;
	struct drm_property *prop_topology_name;
	struct drm_property *prop_connector_caps;
	struct drm_property *prop_mode_info;
	struct drm_property *prop_zpos;
	struct drm_property *prop_alpha;
	struct drm_property *prop_input_fence;
	struct drm_property *prop_blend_op;
	struct drm_property *prop_src_config;
	struct drm_property *prop_color_fill;
	struct drm_property *prop_scaler;
	struct drm_property *prop_csc;
	struct drm_property *prop_plane_caps;
	struct drm_property *prop_master_plane_id;
	struct drm_property *prop_multirect_mode;
	struct drm_property *prop_fb_translation_mode;
	struct drm_property *prop_input_fence_timeout;
	struct drm_property *prop_output_fence;
	struct drm_property *prop_output_fence_offset;
	struct drm_property *prop_crtc_caps;

	uint32_t pending_crtcs;
	wait_queue_head_t pending_crtcs_event;

	struct drm_atomic_state *suspend_state;
	struct drm_client_dev client;

	struct blocking_notifier_head component_notifier_list;
};

void msm_hyp_set_kms(struct drm_device *dev, struct msm_hyp_kms *kms);
void msm_hyp_crtc_commit_done(struct drm_crtc *crtc);
void msm_hyp_crtc_vblank_done(struct drm_crtc *crtc);

#if IS_ENABLED(CONFIG_DRM_MSM_HYP_WFD)
void __init wfd_kms_register(void);
void __exit wfd_kms_unregister(void);
#else
static inline void __init wfd_kms_register(void)
{
}
static inline void __exit wfd_kms_unregister(void)
{
}
#endif /* CONFIG_DRM_MSM_DSI */

#endif /* __MSM_DRV_HYP_H__ */
