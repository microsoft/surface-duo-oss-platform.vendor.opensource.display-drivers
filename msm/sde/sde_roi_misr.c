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

#include <drm/drm_encoder.h>
#include "sde_trace.h"
#include "sde_core_irq.h"
#include "sde_roi_misr.h"
#include "sde_roi_misr_helper.h"

#if defined(CONFIG_DRM_SDE_ROI_MISR)

static void sde_roi_misr_work(struct kthread_work *work);

void sde_roi_misr_init(struct sde_crtc *sde_crtc)
{
	/* Initiate ROI MISR related object */
	INIT_LIST_HEAD(&sde_crtc->roi_misr_data.roi_misr_fence);
	spin_lock_init(&sde_crtc->roi_misr_data.misr_fence_lock);
	kthread_init_work(&sde_crtc->roi_misr_data.misr_event.work,
				sde_roi_misr_work);
}

static inline struct sde_kms *_sde_misr_get_kms(struct drm_crtc *crtc)
{
	struct msm_drm_private *priv;

	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid crtc\n");
		return NULL;
	}
	priv = crtc->dev->dev_private;
	if (!priv || !priv->kms) {
		SDE_ERROR("invalid kms\n");
		return NULL;
	}

	return to_sde_kms(priv->kms);
}

void sde_roi_misr_fence_cleanup(struct sde_crtc *sde_crtc)
{
	struct sde_misr_fence *cur, *tmp;
	unsigned long irq_flags;

	if (list_empty(&sde_crtc->roi_misr_data.roi_misr_fence))
		return;

	spin_lock_irqsave(&sde_crtc->roi_misr_data.misr_fence_lock,
		irq_flags);

	list_for_each_entry_safe(cur, tmp,
		&sde_crtc->roi_misr_data.roi_misr_fence, node)
		del_fence_object(cur);

	spin_unlock_irqrestore(&sde_crtc->roi_misr_data.misr_fence_lock,
		irq_flags);
}

void sde_roi_misr_prepare_fence(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate)
{
	uint64_t fence_fd;
	struct sde_misr_fence *misr_fence;
	struct sde_kms *kms;
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	unsigned long irq_flags;
	int ret;

	kms = _sde_misr_get_kms(&sde_crtc->base);
	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;

	if (!kms->catalog->has_roi_misr)
		return;

	/**
	 * if user_fence_fd_addr value equal NULL,
	 * that means user has not set the ROI_MISR property
	 *
	 * if roi_rect_num equal to zero, we should disable all
	 * misr irqs, so don't need to create fence instance
	 */
	if (!roi_misr_cfg->user_fence_fd_addr
		|| !roi_misr_cfg->roi_rect_num)
		return;

	ret = misr_fence_create(&misr_fence, &fence_fd);
	if (ret < 0) {
		SDE_ERROR("roi msir fence create failed rc:%d\n", ret);
		fence_fd = -1;
	}

	ret = copy_to_user(
		(uint64_t __user *)roi_misr_cfg->user_fence_fd_addr,
		&fence_fd, sizeof(uint64_t));
	if (fence_fd == -1)
		goto exit;
	else if (ret) {
		SDE_ERROR("copy misr fence_fd to user failed rc:%d\n", ret);
		put_unused_fd(fence_fd);
		misr_fence_put(misr_fence);
		goto exit;
	}

	spin_lock_irqsave(&sde_crtc->roi_misr_data.misr_fence_lock,
			irq_flags);
	add_fence_object(&misr_fence->node,
			&sde_crtc->roi_misr_data.roi_misr_fence);
	spin_unlock_irqrestore(&sde_crtc->roi_misr_data.misr_fence_lock,
			irq_flags);

exit:
	return;
}

int sde_roi_misr_cfg_set(struct drm_crtc_state *state,
		void __user *usr_ptr)
{
	struct drm_crtc *crtc;
	struct sde_crtc_state *cstate;
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	struct sde_drm_roi_misr_v1 roi_misr_info;

	if (!state) {
		SDE_ERROR("invalid args\n");
		return -EINVAL;
	}

	if (!usr_ptr) {
		SDE_DEBUG("roi misr cleared\n");
		return 0;
	}

	cstate = to_sde_crtc_state(state);
	crtc = cstate->base.crtc;
	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;

	if (copy_from_user(&roi_misr_info, usr_ptr, sizeof(roi_misr_info))) {
		SDE_ERROR("crtc%d: failed to copy roi_v1 data\n", DRMID(crtc));
		return -EINVAL;
	}

	/* if roi count is zero, we only disable misr irq */
	if (roi_misr_info.roi_rect_num == 0)
		return 0;

	if (roi_misr_info.roi_rect_num > ROI_MISR_MAX_ROIS_PER_CRTC) {
		SDE_ERROR("roi_rect_num greater then maximum roi number\n");
		return -EINVAL;
	}

	if (!roi_misr_info.roi_ids || !roi_misr_info.roi_rects) {
		SDE_ERROR("crtc%d: misr data pointer is NULL\n", DRMID(crtc));
		return -EINVAL;
	}

	roi_misr_cfg->user_fence_fd_addr = roi_misr_info.fence_fd_ptr;
	if (!roi_misr_cfg->user_fence_fd_addr) {
		SDE_ERROR("crtc%d: fence fd address error\n", DRMID(crtc));
		return -EINVAL;
	}

	roi_misr_cfg->roi_rect_num = roi_misr_info.roi_rect_num;

	if (copy_from_user(roi_misr_cfg->roi_ids,
		(void __user *)roi_misr_info.roi_ids,
		sizeof(int) * roi_misr_info.roi_rect_num)) {
		SDE_ERROR("crtc%d: failed to copy roi_ids data\n", DRMID(crtc));
		return -EINVAL;
	}

	if (copy_from_user(roi_misr_cfg->roi_rects,
		(void __user *)roi_misr_info.roi_rects,
		sizeof(struct sde_rect) * roi_misr_info.roi_rect_num)) {
		SDE_ERROR("crtc%d: failed to copy roi_rects data\n",
				DRMID(crtc));
		return -EINVAL;
	}

	/**
	 * if user don't set golden value, always set all
	 * golden values to 0xFFFFFFFF as default value
	 */
	if (!roi_misr_info.roi_golden_value) {
		memset(roi_misr_cfg->roi_golden_value, 0xFF,
			sizeof(roi_misr_cfg->roi_golden_value));
	} else if (copy_from_user(roi_misr_cfg->roi_golden_value,
		(void __user *)roi_misr_info.roi_golden_value,
		sizeof(uint32_t) * roi_misr_info.roi_rect_num)) {
		SDE_ERROR("crtc%d: failed to copy roi_golden_value data\n",
				DRMID(crtc));
		return -EINVAL;
	}

	return 0;
}

static int sde_roi_misr_cfg_check(struct sde_crtc_state *cstate)
{
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	struct drm_clip_rect *roi_range;
	int roi_id;
	int i;

	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;

	if (roi_misr_cfg->roi_rect_num
	    > cstate->misr_state.num_misrs * ROI_MISR_MAX_ROIS_PER_MISR) {
		SDE_ERROR("roi_rect_num(%d) is invalid\n",
				roi_misr_cfg->roi_rect_num);
		return -EINVAL;
	}

	for (i = 0; i < roi_misr_cfg->roi_rect_num; ++i) {
		roi_id = roi_misr_cfg->roi_ids[i];
		roi_range = &cstate->misr_state.roi_range[roi_id];

		if (roi_misr_cfg->roi_rects[i].x1 < roi_range->x1
			|| roi_misr_cfg->roi_rects[i].y1 < roi_range->y1
			|| roi_misr_cfg->roi_rects[i].x2 > roi_range->x2
			|| roi_misr_cfg->roi_rects[i].y2 > roi_range->y2) {
			SDE_ERROR("error rect_info[%d]: {%d,%d,%d,%d}\n",
				i, roi_misr_cfg->roi_rects[i].x1,
				roi_misr_cfg->roi_rects[i].y1,
				roi_misr_cfg->roi_rects[i].x2,
				roi_misr_cfg->roi_rects[i].y2);

			return -EINVAL;
		}
	}

	return 0;
}

static void sde_roi_misr_calc_roi_range(struct drm_crtc_state *state)
{
	struct sde_crtc_state *cstate;
	struct sde_kms *sde_kms;
	struct drm_clip_rect *roi_range;
	struct drm_display_mode drm_mode;
	int all_roi_num;
	int misr_width;
	int i;

	cstate = to_sde_crtc_state(state);
	sde_kms = _sde_misr_get_kms(cstate->base.crtc);

	memset(cstate->misr_state.roi_range, 0,
			sizeof(cstate->misr_state.roi_range));

	if (cstate->num_ds_enabled) {
		SDE_INFO("can't support roi misr with scaler enabled\n");
		cstate->misr_state.num_misrs = 0;

		return;
	}

	cstate->misr_state.num_misrs =
			sde_rm_get_roi_misr_num(&sde_kms->rm,
			cstate->topology_name);
	if (cstate->misr_state.num_misrs == 0)
		return;

	drm_mode = state->adjusted_mode;
	misr_width = drm_mode.hdisplay / cstate->misr_state.num_misrs;
	cstate->misr_state.mixer_width =
			drm_mode.hdisplay / cstate->num_mixers;

	all_roi_num = cstate->misr_state.num_misrs
			* ROI_MISR_MAX_ROIS_PER_MISR;

	for (i = 0; i < all_roi_num; i++) {
		roi_range = &cstate->misr_state.roi_range[i];
		roi_range->x1 = misr_width * (i / ROI_MISR_MAX_ROIS_PER_MISR);
		roi_range->y1 = 0;
		roi_range->x2 = roi_range->x1 + misr_width - 1;
		roi_range->y2 = drm_mode.vdisplay - 1;
	}
}

int sde_roi_misr_check_rois(struct drm_crtc_state *state)
{
	struct sde_crtc_state *crtc_state;
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	int ret;

	if (!state)
		return -EINVAL;

	crtc_state = to_sde_crtc_state(state);
	roi_misr_cfg = &crtc_state->misr_state.roi_misr_cfg;

	/* rebuild roi range table based on current mode */
	if (drm_atomic_crtc_needs_modeset(&crtc_state->base))
		sde_roi_misr_calc_roi_range(state);

	/**
	 * if user_fence_fd_addr is NULL, that means
	 * user has not set the ROI_MISR property
	 */
	if (!roi_misr_cfg->user_fence_fd_addr)
		return 0;

	/**
	 * user can't get roi range through mode_properties
	 * if no available roi misr in current topology,
	 * so user shouldn't set ROI_MISR info
	 */
	if (!crtc_state->misr_state.num_misrs) {
		SDE_ERROR("roi misr is not supported on this topology\n");
		return -EINVAL;
	}

	ret = sde_roi_misr_cfg_check(crtc_state);

	return ret;
}

static void sde_roi_misr_fence_signal(struct sde_crtc *sde_crtc)
{
	unsigned long irq_flags;
	struct sde_misr_fence *fence = get_fence_instance(
			&sde_crtc->roi_misr_data.roi_misr_fence);

	if (!fence) {
		SDE_ERROR("crtc%d: can't get roi misr fence instance!\n",
				sde_crtc->base.base.id);
		return;
	}

	misr_fence_signal(fence);

	spin_lock_irqsave(&sde_crtc->roi_misr_data.misr_fence_lock,
			irq_flags);
	del_fence_object(fence);
	spin_unlock_irqrestore(&sde_crtc->roi_misr_data.misr_fence_lock,
			irq_flags);
}

static void sde_roi_misr_event_cb(void *data, u32 event)
{
	struct drm_crtc *crtc;
	struct sde_crtc *sde_crtc;
	struct msm_drm_private *priv;
	struct sde_crtc_misr_event *misr_event;
	u32 crtc_id;

	if (!data) {
		SDE_ERROR("invalid data parameters\n");
		return;
	}

	crtc = (struct drm_crtc *)data;
	if (!crtc || !crtc->dev || !crtc->dev->dev_private) {
		SDE_ERROR("invalid crtc parameters\n");
		return;
	}

	sde_crtc = to_sde_crtc(crtc);
	priv = crtc->dev->dev_private;
	crtc_id = drm_crtc_index(crtc);

	SDE_DEBUG("crtc%d\n", crtc->base.id);
	SDE_EVT32_VERBOSE(DRMID(crtc), event);

	misr_event = &sde_crtc->roi_misr_data.misr_event;
	misr_event->event = event;
	misr_event->crtc = crtc;
	kthread_queue_work(&priv->event_thread[crtc_id].worker,
			&misr_event->work);
}

static void sde_roi_misr_work(struct kthread_work *work)
{
	struct sde_crtc_misr_event *misr_event;
	struct drm_crtc *crtc;
	struct sde_crtc *sde_crtc;

	if (!work) {
		SDE_ERROR("invalid work handle\n");
		return;
	}

	misr_event = container_of(work, struct sde_crtc_misr_event, work);
	if (!misr_event->crtc || !misr_event->crtc->state) {
		SDE_ERROR("invalid crtc\n");
		return;
	}

	crtc = misr_event->crtc;
	sde_crtc = to_sde_crtc(crtc);

	SDE_ATRACE_BEGIN("crtc_frame_event");

	SDE_DEBUG("crtc%d event:%u\n", crtc->base.id, misr_event->event);

	if (misr_event->event & SDE_ENCODER_MISR_EVENT_SIGNAL_ROI_MSIR_FENCE)
		sde_roi_misr_fence_signal(sde_crtc);

	SDE_ATRACE_END("crtc_frame_event");
}

static void sde_roi_misr_roi_calc(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate)
{
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	struct sde_roi_misr_hw_cfg *roi_misr_hw_cfg;
	struct drm_clip_rect *roi_range;
	int roi_id;
	int misr_idx;
	int misr_roi_idx;
	int i;

	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;

	memset(sde_crtc->roi_misr_data.roi_misr_hw_cfg, 0,
		sizeof(sde_crtc->roi_misr_data.roi_misr_hw_cfg));

	for (i = 0; i < roi_misr_cfg->roi_rect_num; ++i) {
		roi_id = roi_misr_cfg->roi_ids[i];
		roi_range = &cstate->misr_state.roi_range[roi_id];
		misr_idx = roi_id / ROI_MISR_MAX_ROIS_PER_MISR;
		roi_misr_hw_cfg =
			&sde_crtc->roi_misr_data.roi_misr_hw_cfg[misr_idx];
		misr_roi_idx = roi_misr_hw_cfg->roi_num;

		roi_misr_hw_cfg->misr_roi_rect[misr_roi_idx].x =
			roi_misr_cfg->roi_rects[i].x1 - roi_range->x1;
		roi_misr_hw_cfg->misr_roi_rect[misr_roi_idx].y =
			roi_misr_cfg->roi_rects[i].y1;
		roi_misr_hw_cfg->misr_roi_rect[misr_roi_idx].w =
			roi_misr_cfg->roi_rects[i].x2
			- roi_misr_cfg->roi_rects[i].x1 + 1;
		roi_misr_hw_cfg->misr_roi_rect[misr_roi_idx].h =
			roi_misr_cfg->roi_rects[i].y2
			- roi_misr_cfg->roi_rects[i].y1 + 1;

		roi_misr_hw_cfg->golden_value[misr_roi_idx] =
			roi_misr_cfg->roi_golden_value[i];

		/* always set frame_count to one */
		roi_misr_hw_cfg->frame_count[misr_roi_idx] = 1;

		/* record original roi order for return signature */
		roi_misr_hw_cfg->orig_order[misr_roi_idx] = i;

		++roi_misr_hw_cfg->roi_num;
	}
}


static void sde_roi_misr_dspp_roi_calc(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate)
{
	const int dual_mixer = 2;
	struct sde_rect roi_info;
	struct sde_rect *left_rect, *right_rect;
	struct sde_roi_misr_hw_cfg *misr_hw_cfg;
	struct sde_roi_misr_hw_cfg *l_dspp_hw_cfg, *r_dspp_hw_cfg;
	int mixer_width;
	int lms_per_misr;
	int l_roi, r_roi;
	int l_idx, r_idx;
	int i, j;

	lms_per_misr = cstate->num_mixers / cstate->misr_state.num_misrs;
	mixer_width = cstate->misr_state.mixer_width;

	for (i = 0; i < cstate->misr_state.num_misrs; ++i) {
		misr_hw_cfg = &sde_crtc->roi_misr_data.roi_misr_hw_cfg[i];

		/**
		 * Convert MISR rect info to DSPP bypass rect
		 * this rect coordinate has been converted to
		 * every MISR's coordinate, so we can use it
		 * directly. Left and right are abstract concepts,
		 * not specific LM, it is based on one MISR.
		 *
		 * if not in merge mode, only left can be used.
		 *
		 * if in merge mode, left & right are based on
		 * the same MISR.
		 */
		l_idx = (lms_per_misr == dual_mixer)
				? lms_per_misr * i : i;
		l_dspp_hw_cfg =
			&sde_crtc->roi_misr_data.roi_misr_hw_cfg[l_idx];

		r_idx = l_idx + 1;
		r_dspp_hw_cfg =
			&sde_crtc->roi_misr_data.roi_misr_hw_cfg[r_idx];

		for (j = 0; j < misr_hw_cfg->roi_num; ++j) {
			roi_info = misr_hw_cfg->misr_roi_rect[j];

			l_roi = l_dspp_hw_cfg->dspp_roi_num;
			left_rect = &l_dspp_hw_cfg->dspp_roi_rect[l_roi];

			r_roi = r_dspp_hw_cfg->dspp_roi_num;
			right_rect = &r_dspp_hw_cfg->dspp_roi_rect[r_roi];

			if ((roi_info.x + roi_info.w <= mixer_width)) {
				left_rect->x = roi_info.x;
				left_rect->y = roi_info.y;
				left_rect->w = roi_info.w;
				left_rect->h = roi_info.h;
				l_dspp_hw_cfg->dspp_roi_num++;
			} else if (roi_info.x >= mixer_width) {
				right_rect->x = roi_info.x - mixer_width;
				right_rect->y = roi_info.y;
				right_rect->w = roi_info.w;
				right_rect->h = roi_info.h;
				r_dspp_hw_cfg->dspp_roi_num++;
			} else if (lms_per_misr == dual_mixer) {
				left_rect->x = roi_info.x;
				left_rect->y = roi_info.y;
				left_rect->w = mixer_width - left_rect->x;
				left_rect->h = roi_info.h;
				l_dspp_hw_cfg->dspp_roi_num++;

				right_rect->x = 0;
				right_rect->y = roi_info.y;
				right_rect->w = roi_info.w - left_rect->w;
				right_rect->h = roi_info.h;
				r_dspp_hw_cfg->dspp_roi_num++;
			}
		}
	}
}

static bool sde_roi_misr_dspp_is_used(struct sde_crtc *sde_crtc)
{
	int i;

	for (i = 0; i < sde_crtc->num_mixers; ++i)
		if (sde_crtc->mixers[i].hw_dspp)
			return true;

	return false;
}

void sde_roi_misr_setup(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_roi_misr *hw_misr;
	struct sde_hw_dspp *hw_dspp;
	struct sde_roi_misr_hw_cfg *misr_hw_cfg;
	struct sde_kms *kms;
	struct sde_misr_fence *misr_fence;
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	struct sde_ctl_dsc_cfg dsc_cfg;
	int misr_cfg_idx = 0;
	int orig_order_idx;
	int i;

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	kms = _sde_misr_get_kms(crtc);
	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;

	if (!kms->catalog->has_roi_misr)
		return;

	/* do nothing if user didn't set misr */
	if (!roi_misr_cfg->user_fence_fd_addr)
		return;

	misr_fence = get_fence_instance(
			&sde_crtc->roi_misr_data.roi_misr_fence);
	if (!misr_fence && (roi_misr_cfg->roi_rect_num > 0)) {
		SDE_ERROR("crtc%d: can't find fence\n",
				crtc->base.id);
		return;
	}

	memset(&dsc_cfg, 0, sizeof(dsc_cfg));

	sde_roi_misr_roi_calc(sde_crtc, cstate);

	if (sde_roi_misr_dspp_is_used(sde_crtc))
		sde_roi_misr_dspp_roi_calc(sde_crtc, cstate);

	sde_encoder_register_roi_misr_callback(
			sde_crtc->mixers[0].encoder,
			sde_roi_misr_event_cb, crtc);

	for (i = 0; i < sde_crtc->num_mixers; ++i) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		hw_ctl = sde_crtc->mixers[i].hw_ctl;
		hw_misr = sde_crtc->mixers[i].hw_roi_misr;

		if (!hw_misr)
			continue;

		if (misr_cfg_idx >= cstate->misr_state.num_misrs)
			SDE_ERROR("crtc%d: misr config index error\n",
					crtc->base.id);

		misr_hw_cfg =
			&sde_crtc->roi_misr_data.roi_misr_hw_cfg[misr_cfg_idx];

		/* update fence data */
		misr_fence->roi_num[misr_cfg_idx] = misr_hw_cfg->roi_num;
		orig_order_idx = misr_cfg_idx * ROI_MISR_MAX_ROIS_PER_MISR;
		memcpy(&misr_fence->orig_order[orig_order_idx],
			misr_hw_cfg->orig_order,
			sizeof(misr_hw_cfg->orig_order));

		if (hw_dspp) {
			hw_dspp->ops.setup_roi_misr(hw_dspp,
					misr_hw_cfg->dspp_roi_num,
					misr_hw_cfg->dspp_roi_rect);

			hw_ctl->ops.update_bitmask_dspp(hw_ctl,
					hw_dspp->idx, true);
		}

		hw_misr->ops.setup_roi_misr(hw_misr, misr_hw_cfg);
		hw_ctl->ops.update_bitmask_dsc(hw_ctl,
				(enum sde_dsc)hw_misr->idx, true);

		dsc_cfg.dsc[dsc_cfg.dsc_count++] = (enum sde_dsc)hw_misr->idx;

		SDE_DEBUG("crtc%d: setup roi misr, index(%d),",
				crtc->base.id, misr_cfg_idx);
		SDE_DEBUG("roi_num(%d), hw_lm_id %d, hw_misr_id %d\n",
				misr_hw_cfg->roi_num,
				sde_crtc->mixers[i].hw_lm->idx,
				hw_misr->idx);

		++misr_cfg_idx;
	}

	hw_ctl->ops.setup_dsc_cfg(hw_ctl, &dsc_cfg);
}

unsigned int sde_roi_misr_get_num(
		struct drm_encoder *drm_enc)
{
	struct sde_misr_enc_data *misr_data;

	misr_data = sde_encoder_get_misr_data(drm_enc);

	return misr_data->num_roi_misrs;
}

void sde_roi_misr_hw_reset(
		struct sde_encoder_phys *phys_enc,
		struct drm_encoder *base_drm_enc)
{
	struct sde_misr_enc_data *misr_data;
	struct sde_hw_roi_misr *hw_roi_misr;
	int i;

	misr_data = sde_encoder_get_misr_data(base_drm_enc);

	for (i = 0; i < misr_data->num_roi_misrs; i++) {
		hw_roi_misr = misr_data->hw_roi_misr[i];
		if (hw_roi_misr == NULL)
			break;

		if (hw_roi_misr->ops.reset_roi_misr)
			hw_roi_misr->ops.reset_roi_misr(hw_roi_misr);

		phys_enc->hw_ctl->ops.update_bitmask_dsc(
				phys_enc->hw_ctl,
				(enum sde_dsc)hw_roi_misr->idx,
				true);
	}
}

void sde_roi_misr_setup_irq_hw_idx(
		struct sde_encoder_phys *phys_enc,
		struct drm_encoder *base_drm_enc)
{
	struct sde_encoder_irq *irq;
	struct sde_misr_enc_data *misr_data;
	int mismatch_idx;
	int i, j;

	misr_data = sde_encoder_get_misr_data(base_drm_enc);

	for (i = 0; i < misr_data->num_roi_misrs; i++) {
		if (!misr_data->hw_roi_misr[i])
			break;

		for (j = 0; j < ROI_MISR_MAX_ROIS_PER_MISR; j++) {
			mismatch_idx = MISR_ROI_MISMATCH_BASE_IDX
				+ i * ROI_MISR_MAX_ROIS_PER_MISR + j;
			irq = &phys_enc->irq[mismatch_idx];
			if (irq->irq_idx < 0)
				irq->hw_idx = misr_data->hw_roi_misr[i]->idx;
		}
	}
}

int sde_roi_misr_irq_control(struct sde_encoder_phys *phys_enc,
		int base_irq_idx, int roi_idx, bool enable)
{
	struct sde_encoder_irq *irq;
	int irq_tbl_idx;
	int ret;

	if (!phys_enc) {
		SDE_ERROR("invalid parameters\n");
		return -EINVAL;
	}

	irq_tbl_idx = base_irq_idx + roi_idx;
	irq = &phys_enc->irq[irq_tbl_idx];

	if ((irq->irq_idx >= 0) && enable) {
		SDE_DEBUG(
			"skipping already registered irq %s type %d\n",
			irq->name, irq->intr_type);
		return 0;
	}

	if ((irq->irq_idx < 0) && (!enable))
		return 0;

	irq->irq_idx = sde_core_irq_idx_lookup(phys_enc->sde_kms,
			irq->intr_type, irq->hw_idx) + roi_idx;

	if (enable) {
		ret = sde_core_irq_register_callback(phys_enc->sde_kms,
				irq->irq_idx,
				&phys_enc->irq[irq->intr_idx].cb);
		if (ret) {
			SDE_ERROR("failed to register IRQ[%d]\n",
				irq->irq_idx);
			return ret;
		}

		ret = sde_core_irq_enable(phys_enc->sde_kms,
				&irq->irq_idx, 1);
		if (ret) {
			SDE_ERROR("enable irq[%d] error %d\n",
				irq->irq_idx, ret);

			sde_core_irq_unregister_callback(
				phys_enc->sde_kms, irq->irq_idx,
				&phys_enc->irq[irq->intr_idx].cb);
			return ret;
		}
	} else {
		sde_core_irq_disable(phys_enc->sde_kms,
			&irq->irq_idx, 1);

		sde_core_irq_unregister_callback(
			phys_enc->sde_kms, irq->irq_idx,
			&phys_enc->irq[irq->intr_idx].cb);

		irq->irq_idx = -EINVAL;
	}

	return 0;
}

bool sde_roi_misr_update_fence(
		struct sde_encoder_phys *phys_enc,
		struct drm_encoder *base_drm_enc)
{
	struct sde_misr_enc_data *misr_data;
	struct sde_misr_fence *fence;
	struct sde_hw_roi_misr *hw_misr;
	uint32_t signature;
	int all_roi_num = 0;
	int orig_order_idx;
	int orig_order;
	bool success;
	int i, j;

	misr_data = sde_encoder_get_misr_data(base_drm_enc);

	fence = sde_encoder_get_fence_object(phys_enc->parent);
	if (!fence)
		return false;

	/* if has sent event to this fence, we should not send again */
	if (test_bit(MISR_FENCE_FLAG_EVENT_BIT, &fence->flags))
		return false;

	for (i = 0; i < misr_data->num_roi_misrs; ++i) {
		if (!misr_data->hw_roi_misr[i])
			break;

		hw_misr = misr_data->hw_roi_misr[i];

		for (j = 0; j < ROI_MISR_MAX_ROIS_PER_MISR; ++j) {
			success = hw_misr->ops.collect_roi_misr_signature(
				hw_misr, j, &signature);
			if (success) {
				orig_order_idx =
					i * ROI_MISR_MAX_ROIS_PER_MISR + j;
				orig_order = fence->orig_order[orig_order_idx];
				fence->data[orig_order] = signature;
				++fence->updated_count;
			}
		}
	}

	for (i = 0; i < ROI_MISR_MAX_MISRS_PER_CRTC; ++i)
		all_roi_num += fence->roi_num[i];

	if (fence->updated_count >= all_roi_num) {
		set_bit(MISR_FENCE_FLAG_EVENT_BIT, &fence->flags);

		if (fence->updated_count > all_roi_num)
			SDE_ERROR("roi misr issue, please check!\n");

		return true;
	}

	return false;
}

#endif

