// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/uaccess.h>
#include "sde_hw_mdss.h"
#include "sde_fence_post_commit.h"
#include "sde_fence_misr.h"
#include "sde_roi_misr_helper.h"

/**
 * struct sde_roi_misr_hw - store hardware related info
 * @hw_handle: the pointer of misr hardware handle
 * @hw_roi_idx: the roi hardware index
 */
struct sde_roi_misr_hw {
	struct sde_hw_roi_misr *hw_handle;
	uint32_t hw_roi_idx;
};

/**
 * struct sde_misr_fence - misr fence structure
 * @base: the sub-fence instance
 * @total_roi_num: the number of total ROIs
 * @misr_hw: hardware related info for every ROI
 * @signature_mask: the mask of updated signature
 * @signature: signature data
 */
struct sde_misr_fence {
	struct sde_sub_fence base;
	uint32_t total_roi_num;
	struct sde_roi_misr_hw misr_hw[ROI_MISR_MAX_ROIS_PER_CRTC];
	uint32_t signature_mask;
	uint32_t signature[ROI_MISR_MAX_ROIS_PER_CRTC];
};

static inline struct sde_misr_fence *to_sde_misr_fence(
		struct sde_sub_fence *sub_fence)
{
	return container_of(sub_fence, struct sde_misr_fence, base);
}

static uint32_t sde_misr_fence_read(struct dma_fence *fence,
		void __user *usr_ptr, size_t len)
{
	struct sde_generic_fence *generic_fence;
	struct sde_post_commit_fence *post_commit_fence;
	struct sde_misr_fence *misr_fence;
	struct sde_sub_fence *sub_fence;
	size_t copy_len;

	if (!fence)
		return 0;

	generic_fence = to_sde_generic_fence(fence);
	post_commit_fence = to_sde_post_commit_fence(generic_fence);
	sub_fence = post_commit_fence->sub_fence[SDE_SUB_FENCE_ROI_MISR];
	misr_fence = to_sde_misr_fence(sub_fence);

	copy_len = misr_fence->total_roi_num * sizeof(uint32_t);
	copy_len = min_t(size_t, copy_len, len);
	if (copy_to_user(usr_ptr, misr_fence->signature, copy_len)) {
		pr_err("%s: failed to copy_to_user()\n", __func__);
		return 0;
	}

	return copy_len;
}

const struct sde_fence_file_ops misr_file_ops = {
	.read = sde_misr_fence_read,
};

static inline struct sde_crtc *get_sde_crtc(
		struct sde_post_commit_fence *post_commit_fence)
{
	struct sde_sub_fence_context **sub_fence_ctx;
	struct sde_misr_crtc_data *misr_data;

	sub_fence_ctx = post_commit_fence->ctx->sub_fence_ctx;
	misr_data = container_of(
			sub_fence_ctx[SDE_SUB_FENCE_ROI_MISR],
			struct sde_misr_crtc_data,
			context);

	return container_of(misr_data, struct sde_crtc, roi_misr_data);
}

static int sde_misr_fence_prepare(
		struct sde_post_commit_fence *post_commit_fence)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *sde_crtc_state;
	struct sde_misr_fence *misr_fence;
	uint64_t *fd_addr;
	int ret;

	misr_fence = kzalloc(sizeof(*misr_fence), GFP_KERNEL);
	if (!misr_fence)
		return -ENOMEM;

	misr_fence->base.parent = post_commit_fence;
	misr_fence->base.file_ops = &misr_file_ops;
	misr_fence->base.type = SDE_SUB_FENCE_ROI_MISR;

	ret = sde_post_commit_add_sub_fence(&misr_fence->base);
	if (ret)
		goto error;

	sde_crtc = get_sde_crtc(post_commit_fence);
	sde_crtc_state = to_sde_crtc_state(sde_crtc->base.state);
	fd_addr = sde_crtc_state->misr_state.roi_misr_cfg.user_fence_fd_addr;

	ret = copy_to_user((uint64_t __user *)fd_addr,
			&misr_fence->base.fd, sizeof(uint64_t));
	if (ret) {
		pr_err("copy fd to user failed rc:%d\n", ret);
		goto cleanup;
	}

	return 0;

cleanup:
	fput(misr_fence->base.file);
	put_unused_fd(misr_fence->base.fd);
error:
	return ret;
}

static bool sde_misr_fence_update(
		struct sde_sub_fence *sub_fence)
{
	struct sde_crtc *sde_crtc;
	struct sde_crtc_state *cstate;
	struct sde_roi_misr_usr_cfg *roi_misr_cfg;
	struct sde_misr_fence *misr_fence;
	int misr_idx;
	unsigned long flags;
	int i;

	if (!sub_fence) {
		pr_err("%s: invalid arg\n", __func__);
		return false;
	}

	sde_crtc = get_sde_crtc(sub_fence->parent);
	cstate = to_sde_crtc_state(sde_crtc->base.state);
	roi_misr_cfg = &cstate->misr_state.roi_misr_cfg;
	misr_fence = to_sde_misr_fence(sub_fence);

	spin_lock_irqsave(&sde_crtc->roi_misr_data.misr_lock, flags);
	atomic_inc(&sde_crtc->roi_misr_data.cfg_refcount);

	for (i = 0; i < roi_misr_cfg->roi_rect_num; ++i) {
		misr_idx = SDE_ROI_MISR_GET_HW_IDX(roi_misr_cfg->roi_ids[i]);
		misr_fence->misr_hw[i].hw_roi_idx = SDE_ROI_MISR_GET_ROI_IDX(
				roi_misr_cfg->roi_ids[i]);

		misr_fence->misr_hw[i].hw_handle =
				sde_crtc->mixers[misr_idx].hw_roi_misr;
		if (!misr_fence->misr_hw[i].hw_handle) {
			pr_err("roi_ids %u, error hw_handle handle\n",
					roi_misr_cfg->roi_ids[i]);
			goto error;
		}

		misr_fence->total_roi_num++;
	}

	sde_roi_misr_setup(&sde_crtc->base);
	spin_unlock_irqrestore(&sde_crtc->roi_misr_data.misr_lock, flags);

	return true;

error:
	misr_fence->total_roi_num = 0;
	atomic_dec(&sde_crtc->roi_misr_data.cfg_refcount);
	spin_unlock_irqrestore(&sde_crtc->roi_misr_data.misr_lock, flags);

	return false;
}

static bool sde_misr_fence_cache_hw_signature(
		struct sde_misr_fence *misr_fence)
{
	struct sde_hw_roi_misr *hw_handle;
	uint32_t signature;
	uint32_t ready_mask;
	bool success;
	int i;

	for (i = 0; i < misr_fence->total_roi_num; ++i) {
		if (misr_fence->signature_mask & BIT(i))
			continue;

		hw_handle = misr_fence->misr_hw[i].hw_handle;
		success = hw_handle->ops.collect_roi_misr_signature(
				hw_handle,
				misr_fence->misr_hw[i].hw_roi_idx,
				&signature);
		if (success) {
			misr_fence->signature[i] = signature;
			misr_fence->signature_mask |= BIT(i);
		}
	}

	ready_mask = BIT(misr_fence->total_roi_num) - 1;
	if (misr_fence->signature_mask == ready_mask)
		return true;

	return false;
}

static inline void sde_misr_fence_cleanup(struct sde_crtc *sde_crtc)
{
	struct sde_hw_roi_misr *hw_handle;
	struct sde_hw_ctl *hw_ctl;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&sde_crtc->roi_misr_data.misr_lock, flags);
	if (atomic_dec_return(&sde_crtc->roi_misr_data.cfg_refcount)) {
		spin_unlock_irqrestore(
				&sde_crtc->roi_misr_data.misr_lock, flags);
		return;
	}

	for (i = 0; i < sde_crtc->num_mixers; i++) {
		hw_handle = sde_crtc->mixers[i].hw_roi_misr;
		hw_ctl = sde_crtc->mixers[i].hw_ctl;

		if (hw_handle && hw_ctl) {
			hw_handle->ops.reset_roi_misr(hw_handle);
			hw_ctl->ops.update_bitmask_dsc(hw_ctl,
					(enum sde_dsc)hw_handle->idx, true);
		}
	}
	spin_unlock_irqrestore(&sde_crtc->roi_misr_data.misr_lock, flags);
}

static bool sde_misr_fence_fill_data(struct sde_sub_fence *sub_fence,
		bool force)
{
	struct sde_crtc *sde_crtc;
	struct sde_misr_fence *misr_fence;
	bool ret;

	if (!sub_fence)
		return force;

	misr_fence = to_sde_misr_fence(sub_fence);
	sde_crtc = get_sde_crtc(sub_fence->parent);

	ret = sde_misr_fence_cache_hw_signature(misr_fence);

	if (ret || force)
		sde_misr_fence_cleanup(sde_crtc);

	return (ret || force);
}

static void sde_misr_fence_release(
		struct sde_sub_fence *sub_fence)
{
	struct sde_misr_fence *misr_fence;

	misr_fence = to_sde_misr_fence(sub_fence);

	kfree(misr_fence);
}

static const struct sde_sub_fence_ops misr_ops = {
	.prepare = sde_misr_fence_prepare,
	.update = sde_misr_fence_update,
	.fill_data = sde_misr_fence_fill_data,
	.release = sde_misr_fence_release,
};

void sde_misr_fence_ctx_init(struct sde_crtc *sde_crtc)
{
	sde_crtc->roi_misr_data.context.ops = &misr_ops;
	sde_crtc->roi_misr_data.context.type = SDE_SUB_FENCE_ROI_MISR;

	sde_post_commit_add_sub_fence_ctx(
			&sde_crtc->post_commit_fence_ctx,
			&sde_crtc->roi_misr_data.context);
}

