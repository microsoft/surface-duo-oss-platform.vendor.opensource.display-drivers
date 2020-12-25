// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_ROI_MISR_H
#define _SDE_ROI_MISR_H

#include "sde_encoder.h"
#include "sde_hw_roi_misr.h"
#include "sde_hw_dspp.h"
#include <drm/sde_drm.h>

#define SDE_ROI_MISR_GET_HW_IDX(id) ((id) / ROI_MISR_MAX_ROIS_PER_MISR)
#define SDE_ROI_MISR_GET_ROI_IDX(id) ((id) % ROI_MISR_MAX_ROIS_PER_MISR)
#define SDE_ROI_MISR_GET_INTR_OFFSET(hw_id) ((hw_id) - ROI_MISR_0)

/**
 * struct sde_crtc_misr_event: stores roi misr event for crtc processing
 * @work:	base work structure
 * @crtc:	Pointer to crtc handling this event
 * @ts:	timestamp at queue entry
 */
struct sde_crtc_misr_event {
	struct kthread_work work;
	struct drm_crtc *crtc;
	ktime_t ts;
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
 * struct sde_roi_misr_res - store hardware related info
 * @hw_misr: the pointer of misr hardware handle
 * @hw_roi_idx: the roi hardware index
 */
struct sde_roi_misr_res {
	struct sde_hw_roi_misr *hw_misr;
	u32 hw_roi_idx;
};

/**
 * struct sde_misr_cfg_data - store serialized misr config
 * and signature related data
 * @seqno: the commit count when add the setting to list
 * @total_roi_num: the number of total rois
 * @misr_res: hardware related info for every roi
 * @signature_mask: the mask of updated signature
 * @signature: signature data
 * @node: attach this config data to config list
 */
struct sde_misr_cfg_data {
	uint32_t seqno;
	uint32_t total_roi_num;
	struct sde_roi_misr_res misr_res[ROI_MISR_MAX_ROIS_PER_CRTC];
	uint32_t signature_mask;
	uint32_t signature[ROI_MISR_MAX_ROIS_PER_CRTC];
	struct list_head node;
};

#define MISR_DATA_POOL_SIZE 2

/**
 * struct sde_misr_cfg_queue - misr config queue structure
 * @free_pool: pre-alloc pool
 * @free_list: available member list
 * @cfg_list: config data list for timeline
 * @queue_lock: spinlock for protecting queue data
 * @cfg_refcount: track misr_cfg refcount
 */
struct sde_misr_cfg_queue {
	struct sde_misr_cfg_data free_pool[MISR_DATA_POOL_SIZE];
	struct list_head free_list;
	struct list_head cfg_list;
	spinlock_t queue_lock;
	atomic_t cfg_refcount;
};

/**
 * sde_misr_crtc_data - sde misr data structure of sde crtc
 * @misr_fence: misr fence context
 * @misr_event: static allocation of in-flight roi misr events
 * @roi_misr_hw_cfg: the roi misr config should be written to register
 * @cfg_queue: misr config queue
 */
struct sde_misr_crtc_data {
	struct sde_generic_fence_context *misr_fence;
	struct sde_crtc_misr_event misr_event;
	struct sde_roi_misr_hw_cfg roi_misr_hw_cfg[ROI_MISR_MAX_MISRS_PER_CRTC];
	struct sde_misr_cfg_queue cfg_queue;
};

/**
 * sde_misr_enc_data - sde misr data structure of virtual encoder
 * @crtc_roi_misr_cb:	Callback into the upper layer / CRTC for
 *			notification of the ROI MISR
 * @crtc_roi_misr_cb_data:	Data from upper layer for ROI MISR notification
 */
struct sde_misr_enc_data {
	void (*crtc_roi_misr_cb)(void *);
	void *crtc_roi_misr_cb_data;
};

#endif /* _SDE_ROI_MISR_H */
