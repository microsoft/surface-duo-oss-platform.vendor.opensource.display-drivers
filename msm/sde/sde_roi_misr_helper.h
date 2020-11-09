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

#ifndef _SDE_ROI_MISR_HELPER_H
#define _SDE_ROI_MISR_HELPER_H

#include "sde_encoder_phys.h"
#include "sde_crtc.h"
#include "sde_fence_helper.h"

#if defined(CONFIG_DRM_SDE_ROI_MISR)

/**
 * sde_roi_misr_init - initialize roi misr related data
 * @sde_crtc: Pointer to sde crtc
 */
void sde_roi_misr_init(struct sde_crtc *sde_crtc);

/**
 * sde_roi_misr_deinit - destroy roi misr related data
 * @sde_crtc: Pointer to sde crtc
 */
void sde_roi_misr_deinit(struct sde_crtc *sde_crtc);

/**
 * sde_roi_misr_prepare_fence - create a fence instance
 *		and add the instance to global list
 * @sde_crtc: Pointer to sde crtc
 * @cstate: Pointer to sde crtc state
 */
void sde_roi_misr_prepare_fence(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate);

/**
 * sde_roi_misr_cfg_set - copy config data of roi misr
 *		to kernel space, then store the config to sde crtc state
 * @state: Pointer to drm crtc state
 * @usr_ptr: Pointer to config data of user space
 */
int sde_roi_misr_cfg_set(struct drm_crtc_state *state,
		void __user *usr_ptr);

/**
 * sde_roi_misr_check_rois - check roi misr config
 * @crtc: Pointer to drm crtc
 * @state: Pointer to drm crtc state
 */
int sde_roi_misr_check_rois(struct drm_crtc_state *state);

/**
 * sde_roi_misr_setup - Set up roi misr block
 * @crtc: Pointer to drm crtc
 */
void sde_roi_misr_setup(struct drm_crtc *crtc);

/**
 * sde_roi_misr_hw_reset - reset roi misr register values
 * @phys_enc: Pointer to physical encoder structure
 */
void sde_roi_misr_hw_reset(struct sde_encoder_phys *phys_enc);

/**
 * sde_roi_misr_setup_irq_hw_idx - setup irq hardware
 *		index for master physical encoder
 * @phys_enc: Pointer to physical encoder structure
 */
void sde_roi_misr_setup_irq_hw_idx(struct sde_encoder_phys *phys_enc);

/**
 * sde_roi_misr_irq_control - enable or disable all irqs
 *		of one misr block
 * @phys_enc: Pointer to physical encoder structure
 * @base_irq_idx: one roi misr's base irq table index
 * @roi_idx: the roi index of one misr
 * @enable: control to enable or disable one misr block irqs
 * @Return: 0 or -ERROR
 */
int sde_roi_misr_irq_control(struct sde_encoder_phys *phys_enc,
		int base_irq_idx, int roi_idx, bool enable);

/**
 * sde_roi_misr_read_signature - get signatures from the signature queue
 * @sde_crtc: Pointer to sde crtc structure
 * @signature: Pointer to the input buffer
 * @len: the length of the input buffer
 * @Return: 0 or the length of copied data
 */
uint32_t sde_roi_misr_read_signature(struct sde_crtc *sde_crtc,
		uint32_t *signature, uint32_t len);

/**
 * sde_roi_misr_update_fence - update fence data
 * @sde_crtc: Pointer to sde crtc structure
 * @force: force to trigger the last fence
 * @Return: true for all signature are ready
 *          false for all signature are not ready
 */
bool sde_roi_misr_update_fence(struct sde_crtc *sde_crtc, bool force);

#else

static inline
void sde_roi_misr_init(struct sde_crtc *sde_crtc)
{
}

static inline
void sde_roi_misr_deinit(struct sde_crtc *sde_crtc)
{
}

static inline
void sde_roi_misr_prepare_fence(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate)
{
}

static inline
int sde_roi_misr_cfg_set(struct drm_crtc_state *state,
		void __user *usr_ptr)
{
	return 0;
}

static inline
int sde_roi_misr_check_rois(struct drm_crtc_state *state)
{
	return 0;
}

static inline
void sde_roi_misr_setup(struct drm_crtc *crtc)
{
}

static inline
void sde_roi_misr_hw_reset(struct sde_encoder_phys *phys_enc)
{
}

static inline
void sde_roi_misr_setup_irq_hw_idx(struct sde_encoder_phys *phys_enc)
{
}

static inline
int sde_roi_misr_irq_control(struct sde_encoder_phys *phys_enc,
		int base_irq_idx, int roi_idx, bool enable)
{
	return 0;
}

static inline
uint32_t sde_roi_misr_read_signature(struct sde_crtc *sde_crtc,
		uint32_t *signature, uint32_t len)
{
	return 0;
}

static inline
bool sde_roi_misr_update_fence(struct sde_crtc *sde_crtc, bool force)
{
	return false;
}

#endif
#endif /* _SDE_ROI_MISR_HELPER_H */
