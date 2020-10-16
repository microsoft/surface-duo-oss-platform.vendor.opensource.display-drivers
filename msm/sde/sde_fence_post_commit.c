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

#include "sde_fence_post_commit.h"

static bool sde_post_commit_fence_is_signaled(struct dma_fence *fence)
{
	struct sde_generic_fence *generic_fence;
	struct sde_post_commit_fence *post_commit_fence;

	generic_fence = to_sde_generic_fence(fence);
	post_commit_fence = to_sde_post_commit_fence(generic_fence);

	pr_debug("post-commit fence signaled state: %d\n",
			post_commit_fence->triggered);

	return post_commit_fence->triggered;
}

static struct file *sde_post_commit_fence_create_file(
		struct dma_fence *fence)
{
	struct sde_fence_file *fence_file;

	fence_file = sde_fence_file_create(fence);
	if (!fence_file)
		return NULL;

	return fence_file->file;
}

static void sde_post_commit_fence_release(struct dma_fence *fence)
{
	struct sde_generic_fence *generic_fence;
	struct sde_post_commit_fence *post_commit_fence;

	generic_fence = to_sde_generic_fence(fence);
	post_commit_fence = to_sde_post_commit_fence(generic_fence);

	kfree(post_commit_fence);
}

static const struct sde_generic_fence_ops fence_ops = {
	.is_signaled = sde_post_commit_fence_is_signaled,
	.create_file = sde_post_commit_fence_create_file,
	.release = sde_post_commit_fence_release,
};

struct sde_generic_fence_context *sde_post_commit_fence_ctx_init(
		struct sde_crtc *sde_crtc)
{
	return sde_generic_fence_ctx_init(sde_crtc->name, NULL, sde_crtc);
}
EXPORT_SYMBOL(sde_post_commit_fence_ctx_init);

struct sde_post_commit_fence *sde_post_commit_fence_create(
		struct sde_generic_fence_context *ctx,
		int64_t *val)
{

	struct sde_post_commit_fence *post_commit_fence;
	struct sde_crtc *sde_crtc;
	int fd;

	if (!ctx || !val) {
		pr_err("%s: invalid argument args\n", __func__);
		return NULL;
	}

	sde_crtc = ctx->private_data;

	post_commit_fence = kzalloc(sizeof(*post_commit_fence), GFP_KERNEL);
	if (!post_commit_fence)
		return NULL;

	fd = sde_generic_fence_init(ctx, &post_commit_fence->base,
			&fence_ops, sde_crtc->output_fence->commit_count);
	if (fd >= 0) {
		*val = fd;
	} else {
		kfree(post_commit_fence);
		post_commit_fence = NULL;
	}

	return post_commit_fence;
}
EXPORT_SYMBOL(sde_post_commit_fence_create);

void sde_trigger_post_commit_fence(
		struct sde_generic_fence_context *ctx)
{
	struct sde_generic_fence *f;
	struct sde_post_commit_fence *post_commit_fence;
	struct sde_fence_file *fence_file;
	struct sde_crtc *sde_crtc;
	unsigned int vsync_count;

	sde_crtc = ctx->private_data;
	vsync_count = sde_crtc->output_fence->done_count;

	spin_lock(&ctx->list_lock);
	list_for_each_entry(f, &ctx->fence_list_head, fence_list) {
		post_commit_fence = to_sde_post_commit_fence(f);

		if (vsync_count > (f->base.seqno + 1)) {
			post_commit_fence->triggered = true;

			continue;
		} else if (vsync_count >= f->base.seqno) {
			if (f->file) {
				fence_file = f->file->private_data;
				sde_fence_file_trigger(fence_file);
			}

			post_commit_fence->triggered = true;
		}

		break;
	}
	spin_unlock(&ctx->list_lock);
}
EXPORT_SYMBOL(sde_trigger_post_commit_fence);

