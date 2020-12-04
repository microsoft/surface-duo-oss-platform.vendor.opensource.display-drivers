// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "sde_kms.h"
#include "sde_hw_mdss.h"
#include "sde_fence_misr.h"
#include "sde_crtc.h"
#include "sde_encoder.h"
#include "sde_roi_misr_helper.h"

/**
 * struct sde_misr_fence - misr fence structure
 * @base: the generic sub-fence object
 * @generic_fence: the generic fence pointer
 * @sign_data_len: the length of signature data, unit: byte
 * @signature: signature data
 */
struct sde_misr_fence {
	struct sde_sub_fence base;
	struct sde_generic_fence *generic_fence;
	size_t sign_data_len;
	uint32_t signature[ROI_MISR_MAX_ROIS_PER_CRTC];
};

static inline struct sde_misr_fence *to_sde_misr_fence(
		struct sde_sub_fence *sub_fence)
{
	return container_of(sub_fence, struct sde_misr_fence, base);
}

static uint32_t sde_misr_fence_read(struct sde_sub_fence *sub_fence,
		void __user *usr_ptr, size_t len)
{
	struct sde_misr_fence *misr_fence;
	size_t copy_len;

	misr_fence = to_sde_misr_fence(sub_fence);

	if (!misr_fence->sign_data_len)
		return 0;

	copy_len = min_t(size_t, misr_fence->sign_data_len, len);
	if (copy_to_user(usr_ptr, misr_fence->signature, copy_len)) {
		pr_err("%s: failed to copy_to_user()\n", __func__);
		return 0;
	}

	return copy_len;
}

static void sde_misr_fence_fill_data(struct sde_sub_fence *sub_fence)
{
	struct sde_generic_fence_context *ctx;
	struct sde_crtc *sde_crtc;
	struct sde_misr_fence *misr_fence;

	misr_fence = to_sde_misr_fence(sub_fence);
	ctx = misr_fence->generic_fence->ctx;
	sde_crtc = ctx->private_data;

	misr_fence->sign_data_len = sde_roi_misr_read_signature(sde_crtc,
			misr_fence->signature,
			sizeof(misr_fence->signature));
}

static void sde_misr_fence_release(
		struct sde_sub_fence *sub_fence)
{
	struct sde_misr_fence *misr_fence;

	misr_fence = to_sde_misr_fence(sub_fence);

	kfree(misr_fence);
}

static const struct sde_sub_fence_cb sub_fence_cb = {
	.read = sde_misr_fence_read,
	.fill_data = sde_misr_fence_fill_data,
	.release = sde_misr_fence_release,
};

int sde_misr_fence_create(struct sde_post_commit_fence *fence)
{
	struct sde_misr_fence *misr_fence;
	struct sde_fence_file *fence_file;
	int ret;

	misr_fence = kzalloc(sizeof(*misr_fence), GFP_KERNEL);
	if (!misr_fence)
		return -ENOMEM;

	misr_fence->base.cb = &sub_fence_cb;
	misr_fence->generic_fence = &fence->base;
	fence_file = fence->base.file->private_data;

	ret = sde_fence_file_add_sub_fence(fence_file, &misr_fence->base);
	if (ret)
		kfree(misr_fence);

	return ret;
}

