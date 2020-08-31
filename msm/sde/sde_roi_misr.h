/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SDE_ROI_MISR_H
#define _SDE_ROI_MISR_H

#include "sde_encoder.h"
#include "sde_hw_roi_misr.h"
#include "sde_hw_dspp.h"
#include "sde_fence_misr.h"
#include <uapi/drm/sde_drm.h>

/**
 * struct sde_crtc_misr_event: stores roi misr event for crtc processing
 * @work:	base work structure
 * @crtc:	Pointer to crtc handling this event
 * @event:	event identifier
 */
struct sde_crtc_misr_event {
	struct kthread_work work;
	struct drm_crtc *crtc;
	u32 event;
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
 * @roi_misr_fence: list of roi_misr_fence for every commit
 * @misr_fence_lock: spinlock around misr fence handling code
 * @misr_event: static allocation of in-flight roi misr events
 * @roi_misr_hw_cfg: the roi misr config should be written to register
 */
struct sde_misr_crtc_data {
	struct list_head roi_misr_fence;
	spinlock_t misr_fence_lock;
	struct sde_crtc_misr_event misr_event;
	struct sde_roi_misr_hw_cfg roi_misr_hw_cfg[ROI_MISR_MAX_MISRS_PER_CRTC];
};

/**
 * sde_misr_enc_data - sde misr data structure of virtual encoder
 * @num_roi_misrs:	Actual number of roi misrs contained.
 * @hw_roi_misr:	Array of ROI MISR block handles used for the display
 * @crtc_roi_misr_cb:	Callback into the upper layer / CRTC for
 *			notification of the ROI MISR
 * @crtc_roi_misr_cb_data:	Data from upper layer for ROI MISR notification
 */
struct sde_misr_enc_data {
	unsigned int num_roi_misrs;
	struct sde_hw_roi_misr *hw_roi_misr[MAX_CHANNELS_PER_ENC];
	void (*crtc_roi_misr_cb)(void *, u32 event);
	void *crtc_roi_misr_cb_data;
};

#endif /* _SDE_ROI_MISR_H */
