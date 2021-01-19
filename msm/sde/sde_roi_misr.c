// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <drm/drm_encoder.h>
#include "sde_trace.h"
#include "sde_core_irq.h"
#include "sde_roi_misr.h"
#include "sde_fence_misr.h"
#include "sde_fence_post_commit.h"
#include "sde_roi_misr_helper.h"

static void sde_roi_misr_work(struct kthread_work *work);

void sde_roi_misr_init(struct sde_crtc *sde_crtc)
{
	struct sde_misr_cfg_queue *cfg_queue;
	int i;

	sde_crtc->roi_misr_data.misr_fence =
			sde_post_commit_fence_ctx_init(sde_crtc);

	/* Initiate MISR config queue related members */
	cfg_queue = &sde_crtc->roi_misr_data.cfg_queue;
	spin_lock_init(&cfg_queue->queue_lock);
	INIT_LIST_HEAD(&cfg_queue->free_list);
	INIT_LIST_HEAD(&cfg_queue->cfg_list);
	atomic_set(&cfg_queue->cfg_refcount, 0);

	for (i = 0; i < MISR_DATA_POOL_SIZE; i++)
		list_add_tail(&cfg_queue->free_pool[i].node,
				&cfg_queue->free_list);

	kthread_init_work(&sde_crtc->roi_misr_data.misr_event.work,
				sde_roi_misr_work);
}

void sde_roi_misr_deinit(struct sde_crtc *sde_crtc)
{
	sde_generic_fence_ctx_deinit(sde_crtc->roi_misr_data.misr_fence);
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

static inline void sde_roi_misr_release_fence_file(
		struct sde_post_commit_fence *post_commit_fence,
		uint64_t *fence_fd)
{
	fput(post_commit_fence->base.file);
	post_commit_fence->base.file = NULL;

	put_unused_fd(*fence_fd);
	*fence_fd = -1;
}

void sde_roi_misr_prepare_fence(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate)
{
	struct sde_kms *kms;
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	struct sde_post_commit_fence *post_commit_fence;
	uint64_t fence_fd;
	int ret;

	kms = _sde_misr_get_kms(&sde_crtc->base);
	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;

	if (!kms || !kms->catalog->has_roi_misr)
		return;

	/**
	 * if user_fence_fd_addr value equal NULL,
	 * that means user has not set the ROI_MISR property
	 *
	 * it is an empty commit if roi_rect_num equal to zero,
	 * so don't need to create fence instance
	 */
	if (!roi_misr_cfg->user_fence_fd_addr
		|| roi_misr_cfg->roi_rect_num == 0)
		return;

	post_commit_fence = sde_post_commit_fence_create(
			sde_crtc->roi_misr_data.misr_fence,
			&fence_fd);
	if (!post_commit_fence) {
		SDE_ERROR("post commit fence create failed\n");
		fence_fd = -1;
	} else {
		ret = sde_misr_fence_create(post_commit_fence);
		if (ret) {
			SDE_ERROR("roi misr fence create failed rc:%d\n", ret);

			sde_roi_misr_release_fence_file(post_commit_fence,
					&fence_fd);
		}
	}

	ret = copy_to_user(
		(uint64_t __user *)roi_misr_cfg->user_fence_fd_addr,
		&fence_fd, sizeof(uint64_t));
	if (ret && (fence_fd >= 0)) {
		SDE_ERROR("copy misr fence_fd to user failed rc:%d\n", ret);

		sde_roi_misr_release_fence_file(post_commit_fence,
				&fence_fd);
	}
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

	/* return directly if the roi_misr commit is empty */
	if (roi_misr_info.roi_rect_num == 0)
		return 0;

	if (roi_misr_info.roi_rect_num > ROI_MISR_MAX_ROIS_PER_CRTC) {
		SDE_ERROR("invalid roi_rect_num(%u)\n",
				roi_misr_info.roi_rect_num);
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
				roi_id,
				roi_misr_cfg->roi_rects[i].x1,
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
	int roi_factor, roi_id;
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

	roi_factor = sde_rm_is_3dmux_case(cstate->topology_name)
			? 2 * ROI_MISR_MAX_ROIS_PER_MISR
			: ROI_MISR_MAX_ROIS_PER_MISR;

	for (i = 0; i < all_roi_num; i++) {
		roi_id = roi_factor * SDE_ROI_MISR_GET_HW_IDX(i)
				+ SDE_ROI_MISR_GET_ROI_IDX(i);

		roi_range = &cstate->misr_state.roi_range[roi_id];
		roi_range->x1 = misr_width * SDE_ROI_MISR_GET_HW_IDX(i);
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

static void sde_roi_misr_event_cb(void *data)
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

	if (!sde_roi_misr_update_fence(sde_crtc, false))
		return;

	misr_event = &sde_crtc->roi_misr_data.misr_event;
	misr_event->crtc = crtc;
	misr_event->ts = ktime_get();
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

	SDE_ATRACE_BEGIN("crtc_roi_misr_event");

	sde_generic_fence_signal(sde_crtc->roi_misr_data.misr_fence);

	SDE_ATRACE_END("crtc_roi_misr_event");
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
		misr_idx = SDE_ROI_MISR_GET_HW_IDX(roi_id);
		roi_misr_hw_cfg =
			&sde_crtc->roi_misr_data.roi_misr_hw_cfg[misr_idx];
		misr_roi_idx = SDE_ROI_MISR_GET_ROI_IDX(roi_id);

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

		roi_misr_hw_cfg->roi_mask |= BIT(misr_roi_idx);
	}
}

static void sde_roi_misr_dspp_roi_calc(struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate)
{
	const int dual_mixer = 2;
	struct sde_rect roi_info;
	struct sde_rect *left_rect, *right_rect;
	struct sde_roi_misr_hw_cfg *l_dspp_hw_cfg, *r_dspp_hw_cfg;
	int mixer_width;
	int lms_per_misr;
	int l_idx, r_idx;
	int i, j;

	lms_per_misr = cstate->num_mixers / cstate->misr_state.num_misrs;
	mixer_width = cstate->misr_state.mixer_width;

	for (i = 0; i < cstate->misr_state.num_misrs; ++i) {
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

		for (j = 0; j < ROI_MISR_MAX_ROIS_PER_MISR; ++j) {
			if (!(l_dspp_hw_cfg->roi_mask & BIT(i)))
				continue;

			roi_info = l_dspp_hw_cfg->misr_roi_rect[j];
			left_rect = &l_dspp_hw_cfg->dspp_roi_rect[j];
			right_rect = &r_dspp_hw_cfg->dspp_roi_rect[j];

			if ((roi_info.x + roi_info.w <= mixer_width)) {
				left_rect->x = roi_info.x;
				left_rect->y = roi_info.y;
				left_rect->w = roi_info.w;
				left_rect->h = roi_info.h;
				l_dspp_hw_cfg->dspp_roi_mask |= BIT(j);
			} else if (roi_info.x >= mixer_width) {
				right_rect->x = roi_info.x - mixer_width;
				right_rect->y = roi_info.y;
				right_rect->w = roi_info.w;
				right_rect->h = roi_info.h;
				r_dspp_hw_cfg->dspp_roi_mask |= BIT(j);
			} else if (lms_per_misr == dual_mixer) {
				left_rect->x = roi_info.x;
				left_rect->y = roi_info.y;
				left_rect->w = mixer_width - left_rect->x;
				left_rect->h = roi_info.h;
				l_dspp_hw_cfg->dspp_roi_mask |= BIT(j);

				right_rect->x = 0;
				right_rect->y = roi_info.y;
				right_rect->w = roi_info.w - left_rect->w;
				right_rect->h = roi_info.h;
				r_dspp_hw_cfg->dspp_roi_mask |= BIT(j);
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

static bool sde_roi_misr_add_new_cfg(
		struct sde_crtc *sde_crtc,
		struct sde_crtc_state *cstate)
{
	struct sde_roi_misr_usr_cfg *misr_user_cfg;
	struct sde_misr_cfg_queue *cfg_queue;
	struct sde_misr_cfg_data *cfg_data;
	int misr_idx;
	int i;

	misr_user_cfg = &cstate->misr_state.roi_misr_cfg;
	cfg_queue = &sde_crtc->roi_misr_data.cfg_queue;

	spin_lock(&cfg_queue->queue_lock);
	cfg_data = list_first_entry_or_null(&cfg_queue->free_list,
			struct sde_misr_cfg_data, node);
	if (!cfg_data) {
		spin_unlock(&cfg_queue->queue_lock);
		SDE_ERROR("get available sde_misr_cfg_data failed\n");

		return false;
	}

	list_del(&cfg_data->node);
	spin_unlock(&cfg_queue->queue_lock);

	memset(cfg_data, 0, sizeof(*cfg_data));

	for (i = 0; i < misr_user_cfg->roi_rect_num; ++i) {
		misr_idx = SDE_ROI_MISR_GET_HW_IDX(misr_user_cfg->roi_ids[i]);
		cfg_data->misr_res[i].hw_roi_idx = SDE_ROI_MISR_GET_ROI_IDX(
				misr_user_cfg->roi_ids[i]);

		cfg_data->misr_res[i].hw_misr =
				sde_crtc->mixers[misr_idx].hw_roi_misr;

		if (!cfg_data->misr_res[i].hw_misr) {
			SDE_ERROR("roi_ids %u, error hw_misr handle\n",
					misr_user_cfg->roi_ids[i]);

			/* add current node back to free list if failed */
			list_add_tail(&cfg_data->node, &cfg_queue->free_list);

			return false;
		}
	}

	cfg_data->total_roi_num = misr_user_cfg->roi_rect_num;
	cfg_data->seqno = sde_crtc->output_fence->commit_count;

	spin_lock(&cfg_queue->queue_lock);
	list_add_tail(&cfg_data->node, &cfg_queue->cfg_list);
	atomic_inc(&cfg_queue->cfg_refcount);
	spin_unlock(&cfg_queue->queue_lock);

	return true;
}

static struct sde_misr_cfg_data *sde_roi_misr_get_active_cfg(
		struct sde_crtc *sde_crtc)
{
	struct sde_misr_cfg_queue *cfg_queue;
	struct sde_misr_cfg_data *cur, *tmp, *valid = NULL;
	unsigned int vsync_count;

	cfg_queue = &sde_crtc->roi_misr_data.cfg_queue;
	vsync_count = sde_crtc->output_fence->done_count;

	list_for_each_entry_safe(cur, tmp, &cfg_queue->cfg_list, node) {
		if (cur->seqno < vsync_count) {
			list_del(&cur->node);
			list_add_tail(&cur->node, &cfg_queue->free_list);
		} else if (cur->seqno == vsync_count) {
			valid = cur;
			break;
		}
	}

	return valid;
}

static inline void sde_roi_misr_reset_misr(struct sde_crtc *sde_crtc)
{
	struct sde_hw_roi_misr *hw_misr;
	struct sde_hw_ctl *hw_ctl;
	int i;

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		hw_misr = sde_crtc->mixers[i].hw_roi_misr;
		hw_ctl = sde_crtc->mixers[i].hw_ctl;

		if (hw_misr && hw_ctl) {
			hw_misr->ops.reset_roi_misr(hw_misr);
			hw_ctl->ops.update_bitmask_dsc(hw_ctl,
					(enum sde_dsc)hw_misr->idx, true);
		}
	}
}

static inline void sde_roi_misr_free_cfg(struct sde_crtc *sde_crtc,
		struct sde_misr_cfg_queue *cfg_queue,
		struct sde_misr_cfg_data *cfg_data)
{
	list_del(&cfg_data->node);
	list_add_tail(&cfg_data->node, &cfg_queue->free_list);

	if (atomic_dec_return(&cfg_queue->cfg_refcount) == 0)
		sde_roi_misr_reset_misr(sde_crtc);
}

void sde_roi_misr_setup(struct drm_crtc *crtc)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct sde_kms *kms;
	struct sde_hw_ctl *hw_ctl;
	struct sde_hw_roi_misr *hw_misr;
	struct sde_hw_dspp *hw_dspp;
	struct sde_roi_misr_hw_cfg *misr_hw_cfg;
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	struct sde_generic_fence_context *misr_fence;
	struct sde_ctl_dsc_cfg dsc_cfg;
	int i;

	sde_crtc = to_sde_crtc(crtc);
	cstate = to_sde_crtc_state(crtc->state);
	kms = _sde_misr_get_kms(crtc);
	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;
	misr_fence = sde_crtc->roi_misr_data.misr_fence;

	if (!kms || !kms->catalog->has_roi_misr)
		return;

	/* do nothing if user didn't set misr */
	if (!roi_misr_cfg->user_fence_fd_addr)
		return;

	if (list_empty(&misr_fence->fence_list_head)) {
		SDE_ERROR("crtc%d: can't find fence instance\n",
				crtc->base.id);
		return;
	}

	if (!sde_roi_misr_add_new_cfg(sde_crtc, cstate)) {
		SDE_ERROR("crtc%d: add roi misr config failed\n",
				crtc->base.id);
		sde_generic_fence_signal(misr_fence);

		return;
	}

	memset(&dsc_cfg, 0, sizeof(dsc_cfg));

	sde_roi_misr_roi_calc(sde_crtc, cstate);

	if (sde_roi_misr_dspp_is_used(sde_crtc))
		sde_roi_misr_dspp_roi_calc(sde_crtc, cstate);

	sde_encoder_register_roi_misr_callback(
			sde_crtc->mixers[0].encoder,
			sde_roi_misr_event_cb, crtc);

	hw_ctl = sde_crtc->mixers[0].hw_ctl;

	for (i = 0; i < sde_crtc->num_mixers; ++i) {
		hw_dspp = sde_crtc->mixers[i].hw_dspp;
		hw_misr = sde_crtc->mixers[i].hw_roi_misr;
		misr_hw_cfg =
			&sde_crtc->roi_misr_data.roi_misr_hw_cfg[i];

		if (hw_dspp) {
			hw_dspp->ops.setup_roi_misr(hw_dspp,
					misr_hw_cfg->dspp_roi_mask,
					misr_hw_cfg->dspp_roi_rect);

			hw_ctl->ops.update_bitmask_dspp(hw_ctl,
					hw_dspp->idx, true);
		}

		if (!hw_misr)
			continue;

		hw_misr->ops.setup_roi_misr(hw_misr, misr_hw_cfg);
		hw_ctl->ops.update_bitmask_dsc(hw_ctl,
				(enum sde_dsc)hw_misr->idx, true);

		dsc_cfg.dsc[dsc_cfg.dsc_count++] = (enum sde_dsc)hw_misr->idx;

		SDE_DEBUG("crtc%d: setup roi misr, index(%d),",
				crtc->base.id, i);
		SDE_DEBUG("roi_mask(%x), hw_lm_id %d, hw_misr_id %d\n",
				misr_hw_cfg->roi_mask,
				sde_crtc->mixers[i].hw_lm->idx,
				hw_misr->idx);
	}

	hw_ctl->ops.setup_dsc_cfg(hw_ctl, &dsc_cfg);
}

void sde_roi_misr_hw_reset(struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_roi_misr *hw_roi_misr;
	int i;

	for (i = 0; i < phys_enc->roi_misr_num; i++) {
		hw_roi_misr = phys_enc->hw_roi_misr[i];
		if (!hw_roi_misr->ops.reset_roi_misr)
			continue;

		hw_roi_misr->ops.reset_roi_misr(hw_roi_misr);
		phys_enc->hw_ctl->ops.update_bitmask_dsc(
				phys_enc->hw_ctl,
				(enum sde_dsc)hw_roi_misr->idx,
				true);
	}
}

void sde_roi_misr_setup_irq_hw_idx(struct sde_encoder_phys *phys_enc)
{
	struct sde_hw_roi_misr *hw_roi_misr;
	struct sde_encoder_irq *irq;
	int mismatch_idx, intr_offset;
	int i, j;

	for (i = 0; i < phys_enc->roi_misr_num; i++) {
		hw_roi_misr = phys_enc->hw_roi_misr[i];
		intr_offset = SDE_ROI_MISR_GET_INTR_OFFSET(hw_roi_misr->idx);

		for (j = 0; j < ROI_MISR_MAX_ROIS_PER_MISR; j++) {
			mismatch_idx = MISR_ROI_MISMATCH_BASE_IDX
				+ intr_offset * ROI_MISR_MAX_ROIS_PER_MISR + j;

			irq = &phys_enc->irq[mismatch_idx];
			if (irq->irq_idx < 0)
				irq->hw_idx = hw_roi_misr->idx;
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

	SDE_DEBUG("hw_idx(%d) roi_idx(%d) irq_idx(%d) enable(%d)\n",
			irq->hw_idx, roi_idx, irq->irq_idx, enable);

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

uint32_t sde_roi_misr_read_signature(struct sde_crtc *sde_crtc,
		uint32_t *signature, uint32_t len)
{
	struct sde_misr_cfg_queue *cfg_queue;
	struct sde_misr_cfg_data *cfg_data;
	uint32_t copy_len;

	cfg_queue = &sde_crtc->roi_misr_data.cfg_queue;

	cfg_data = list_first_entry_or_null(&cfg_queue->cfg_list,
			struct sde_misr_cfg_data, node);
	if (!cfg_data)
		return 0;

	copy_len = cfg_data->total_roi_num * sizeof(uint32_t);
	copy_len = min(copy_len, len);

	memcpy(signature, cfg_data->signature, copy_len);

	return copy_len;
}

static void sde_roi_misr_cache_hw_signature(
		struct sde_misr_cfg_data *cfg_data)
{
	struct sde_hw_roi_misr *hw_misr;
	uint32_t signature;
	bool success;
	int i;

	for (i = 0; i < cfg_data->total_roi_num; ++i) {
		if (cfg_data->signature_mask & BIT(i))
			continue;

		hw_misr = cfg_data->misr_res[i].hw_misr;
		success = hw_misr->ops.collect_roi_misr_signature(
				hw_misr,
				cfg_data->misr_res[i].hw_roi_idx,
				&signature);
		if (success) {
			cfg_data->signature[i] = signature;
			cfg_data->signature_mask |= BIT(i);
		}
	}
}

bool sde_roi_misr_update_fence(struct sde_crtc *sde_crtc,
		bool force)
{
	struct sde_generic_fence_context *fence_ctx;
	struct sde_misr_cfg_queue *cfg_queue;
	struct sde_misr_cfg_data *cfg_data;
	uint32_t ready_mask;

	fence_ctx = sde_crtc->roi_misr_data.misr_fence;
	cfg_queue = &sde_crtc->roi_misr_data.cfg_queue;

	spin_lock(&cfg_queue->queue_lock);
	cfg_data = sde_roi_misr_get_active_cfg(sde_crtc);
	if (!cfg_data) {
		spin_unlock(&cfg_queue->queue_lock);
		return false;
	}

	sde_roi_misr_cache_hw_signature(cfg_data);

	ready_mask = BIT(cfg_data->total_roi_num) - 1;
	if (force || (cfg_data->signature_mask == ready_mask)) {
		sde_trigger_post_commit_fence(fence_ctx);

		sde_roi_misr_free_cfg(sde_crtc, cfg_queue, cfg_data);
		spin_unlock(&cfg_queue->queue_lock);

		if (force)
			sde_generic_fence_signal(fence_ctx);

		return true;
	}

	spin_unlock(&cfg_queue->queue_lock);

	return false;
}

