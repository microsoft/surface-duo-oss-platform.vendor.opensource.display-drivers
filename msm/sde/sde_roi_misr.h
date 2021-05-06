// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_ROI_MISR_H
#define _SDE_ROI_MISR_H

#include <drm/sde_drm.h>
#include "sde_encoder.h"
#include "sde_hw_roi_misr.h"
#include "sde_hw_dspp.h"
#include "sde_fence_post_commit.h"

#define SDE_ROI_MISR_GET_HW_IDX(id) ((id) / ROI_MISR_MAX_ROIS_PER_MISR)
#define SDE_ROI_MISR_GET_ROI_IDX(id) ((id) % ROI_MISR_MAX_ROIS_PER_MISR)
#define SDE_ROI_MISR_GET_INTR_OFFSET(hw_id) ((hw_id) - ROI_MISR_0)

/**
 * struct sde_crtc_misr_event: stores roi misr event for crtc processing
 * @work:	base work structure
 * @crtc:	Pointer to crtc handling this event
 */
struct sde_crtc_misr_event {
	struct kthread_work work;
	struct drm_crtc *crtc;
};

/**
 * sde_misr_state - sde misr state of current topology
 * @num_misrs: Number of roi misrs in current topology
 * @mixer_width: width of every mixer in current topology
 * @roi_misr_cfg: roi misr configuration from user space
 * @roi_range: misr roi range table
 */
struct sde_misr_state {
	u32 num_misrs;
	u32 mixer_width;
	struct sde_roi_misr_usr_cfg roi_misr_cfg;
	struct drm_clip_rect roi_range[ROI_MISR_MAX_ROIS_PER_CRTC];
};

/**
 * sde_misr_crtc_data - sde misr data structure of sde crtc
 * @context: misr fence context
 * @misr_event: static allocation of in-flight roi misr events
 * @roi_misr_hw_cfg: the roi misr config should be written to register
 * @misr_lock: spinlock for protecting misr related critical resource
 * @cfg_refcount: track cfg refcount of roi misr
 */
struct sde_misr_crtc_data {
	struct sde_sub_fence_context context;
	struct sde_crtc_misr_event misr_event;
	struct sde_roi_misr_hw_cfg roi_misr_hw_cfg[ROI_MISR_MAX_MISRS_PER_CRTC];
	spinlock_t misr_lock;
	atomic_t cfg_refcount;
};

/**
 * sde_misr_enc_data - sde misr data structure of virtual encoder
 * @crtc_roi_misr_cb: callback into the upper layer / CRTC for
 *			notification of the ROI MISR
 * @crtc_roi_misr_cb_data: data from upper layer for ROI MISR notification
 */
struct sde_misr_enc_data {
	void (*crtc_roi_misr_cb)(void *);
	void *crtc_roi_misr_cb_data;
};

#endif /* _SDE_ROI_MISR_H */
