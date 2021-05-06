/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <drm/sde_drm.h>
#include "sde_hw_top.h"
#include "shd_drm.h"

#ifndef SHD_HW_H
#define SHD_HW_H

struct sde_shd_ctl_mixer_cfg {
	u32 mixercfg;
	u32 mixercfg_ext;
	u32 mixercfg_ext2;
	u32 mixercfg_ext3;

	u32 mixercfg_mask;
	u32 mixercfg_ext_mask;
	u32 mixercfg_ext2_mask;
	u32 mixercfg_ext3_mask;

	u32 mixercfg_skip_sspp_mask[2];
};

struct sde_shd_hw_ctl {
	struct sde_hw_ctl base;
	struct shd_stage_range range;
	struct sde_hw_ctl *orig;
	u32 flush_mask;
	u32 old_mask;
	struct sde_shd_ctl_mixer_cfg mixer_cfg[MAX_BLOCKS];

	bool cwb_enable;
	bool cwb_changed;
	u32 cwb_active;
	u32 merge_3d_active;

	struct sde_ctl_dsc_cfg dsc_cfg;
};

struct sde_shd_mixer_cfg {
	uint32_t fg_alpha;
	uint32_t bg_alpha;
	uint32_t blend_op;
	bool dirty;

	struct sde_hw_dim_layer dim_layer;
	bool dim_layer_enable;
};

struct sde_shd_hw_mixer {
	struct sde_hw_mixer base;
	struct shd_stage_range range;
	struct sde_rect roi;
	struct sde_hw_mixer *orig;
	struct sde_shd_mixer_cfg cfg[SDE_STAGE_MAX];
};

struct sde_shd_hw_roi_misr {
	struct sde_hw_roi_misr base;
	struct sde_hw_roi_misr *orig;
	struct sde_roi_misr_hw_cfg misr_cfg;
};

void sde_shd_hw_flush(struct sde_hw_ctl *ctl_ctx,
	struct sde_hw_mixer *lm_ctx[MAX_MIXERS_PER_CRTC], int lm_num,
	struct sde_hw_roi_misr *misr_ctx[MAX_MIXERS_PER_CRTC], int misr_num);

void sde_shd_hw_ctl_init_op(struct sde_hw_ctl *ctx);

void sde_shd_hw_lm_init_op(struct sde_hw_mixer *ctx);

void sde_shd_hw_roi_misr_init_op(struct sde_hw_roi_misr *ctx);

void sde_shd_hw_skip_sspp_clear(struct sde_hw_ctl *ctx,
	enum sde_sspp sspp, int multirect_idx);

#endif
