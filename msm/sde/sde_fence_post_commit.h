/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_POST_COMMIT_FENCE_H_
#define _SDE_POST_COMMIT_FENCE_H_

#include "sde_fence_helper.h"
#include "sde_fence_file.h"

struct sde_post_commit_fence;

/**
 * enum sde_sub_fence_type - the supported sub-fence type
 *
 * The new item must be added before SDE_SUB_FENCE_MAX in this list.
 */
enum sde_sub_fence_type {
	SDE_SUB_FENCE_ROI_MISR = 0,
	SDE_SUB_FENCE_MAX
};

/**
 * struct sde_sub_fence - sub-fence structure
 * @parent: the post-commit fence pointer of sub-fence
 * @type: the sub-fence type
 * @file_ops: the operation pointer for sub-fence file
 * @file: the fence file pointer of sub-fence
 * @fd: the file descriptor of sub-fence
 */
struct sde_sub_fence {
	struct sde_post_commit_fence *parent;
	enum sde_sub_fence_type type;
	const struct sde_fence_file_ops *file_ops;
	struct file *file;
	uint64_t fd;
};

/**
 * struct sde_sub_fence_ops - sub-fence operations structure
 * @prepare: the function pointer for creating sub-fences with given type
 * @udpate: the function pointer for update the private data of sub-fence
 * @fill_data: the function pointer for filling read data
 * @release: the function pointer for release sub-fence
 */
struct sde_sub_fence_ops {
	int (*prepare)(struct sde_post_commit_fence *fence);
	bool (*update)(struct sde_sub_fence *sub_fence);
	bool (*fill_data)(struct sde_sub_fence *sub_fence, bool force);
	void (*release)(struct sde_sub_fence *sub_fence);
};

/**
 * struct sde_sub_fence_context - sub-fence context structure
 * @type: the sub-fence type
 * @ops: the operation pointer of sub-fence
 */
struct sde_sub_fence_context {
	enum sde_sub_fence_type type;
	const struct sde_sub_fence_ops *ops;
};

/**
 * struct sde_post_commit_fence_context - post-commit fence context structure
 * @base: the generic fence instance
 * @sub_fence_ctx: the sub-fence context pointer
 * @done_count: done count pointer for timeline check
 */
struct sde_post_commit_fence_context {
	struct sde_generic_fence_context base;
	struct sde_sub_fence_context *sub_fence_ctx[SDE_SUB_FENCE_MAX];
	unsigned int *done_count;
};

/**
 * struct sde_post_commit_fence - sde post-commit fence structure
 * @base: the generic fence object
 * @ctx: the post-commit fence context pointer
 * @trigger_mask: the trigger mask represents which sub-fences
 *                data have been filled
 * @sub_fence_mask: the sub-fence mask represents which sub-fences
 *                  have been created
 * @sub_fence: the pointer of sub_fence instance
 */
struct sde_post_commit_fence {
	struct sde_generic_fence base;
	struct sde_post_commit_fence_context *ctx;
	uint32_t trigger_mask;
	uint32_t sub_fence_mask;
	struct sde_sub_fence *sub_fence[SDE_SUB_FENCE_MAX];
};

static inline struct sde_post_commit_fence *to_sde_post_commit_fence(
		struct sde_generic_fence *fence)
{
	return container_of(fence, struct sde_post_commit_fence, base);
}

/**
 * sde_post_commit_fence_ctx_init - init the post-commit fence context
 * @ctx: the pointer of post-commit fence context
 * @name: the pointer of post-commit fence context name
 * @done_count: the pointer of done count for timeline check
 *
 * Return zero or errno in case of error.
 */
int sde_post_commit_fence_ctx_init(
		struct sde_post_commit_fence_context *ctx,
		const char *name,
		unsigned int *done_count);

/**
 * sde_post_commit_add_sub_fence_ctx - add a sub-fence context
 * @ctx: the pointer of post-commit fence context
 * @name: the pointer of sub-fence context
 *
 * Return zero or errno in case of error.
 */
int sde_post_commit_add_sub_fence_ctx(
		struct sde_post_commit_fence_context *ctx,
		struct sde_sub_fence_context *sub_fence_ctx);

/**
 * sde_post_commit_fence_create - create a post-commit fence
 * @ctx: the pointer of post-commit fence context
 * @post_commit_fence_mask: post-commit fence mask represents
 *                          which sub-fences have been created
 * @val: the initialized value of the sequence number of dma_fence
 */
void sde_post_commit_fence_create(
		struct sde_post_commit_fence_context *ctx,
		uint32_t post_commit_fence_mask,
		unsigned int val);

/**
 * sde_post_commit_add_sub_fence - add sub-fence to post-commit fence
 *
 * @sub_fence: the sub-fence pointer which should be added into
 *             post-commit fence
 *
 * Return zero or errno in case of error.
 */
int sde_post_commit_add_sub_fence(
		struct sde_sub_fence *sub_fence);

/**
 * sde_post_commit_fence_update - update the sub-fence data
 * of post-commit fence. This function should be called when
 * CRTC call atomic_begin callback.
 *
 * @ctx: the fence context pointer
 *
 * Return zero or errno in case of error.
 */
int sde_post_commit_fence_update(struct sde_post_commit_fence_context *ctx);

/**
 * sde_post_commit_signal_fence - call all fill data callbacks
 * of all post-commit sub-fences which meet the conditions, and set
 * the fence flag to signaled state, then call signal fence API.
 *
 * @ctx: the fence context pointer
 */
void sde_post_commit_signal_fence(
		struct sde_post_commit_fence_context *ctx);

/**
 * sde_post_commit_signal_sub_fence - trigger the fill data
 * callback of one specific post-commit sub-fence, then try to
 * signal fence.
 *
 * @ctx: the fence context pointer
 * @type: the type of sub-fence
 */
void sde_post_commit_signal_sub_fence(
		struct sde_post_commit_fence_context *ctx,
		enum sde_sub_fence_type type);

#endif /* _SDE_POST_COMMIT_FENCE_H_ */

