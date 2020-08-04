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

#ifndef _SDE_POST_COMMIT_FENCE_H_
#define _SDE_POST_COMMIT_FENCE_H_

#include "sde_fence_helper.h"
#include "sde_fence_file.h"
#include "sde_crtc.h"

/**
 * struct sde_post_commit_fence - sde post-commit fence structure
 * @base: the generic fence object
 * @triggered: flag this fence is triggered or not
 */
struct sde_post_commit_fence {
	struct sde_generic_fence base;
	bool triggered;
};

static inline struct sde_post_commit_fence *to_sde_post_commit_fence(
		struct sde_generic_fence *fence)
{
	return container_of(fence, struct sde_post_commit_fence, base);
}

/**
 * sde_post_commit_fence_ctx_init - init the post-commit fence context
 * @sde_crtc: pointer of sde_crtc
 *
 * Return generic fence context pointer or ERR_PTR in case of error.
 */
struct sde_generic_fence_context *sde_post_commit_fence_ctx_init(
		struct sde_crtc *sde_crtc);

/**
 * sde_post_commit_fence_create - create a post-commit fence
 * @ctx: the fence context pointer
 * @val: the pointer of buffer to store the fd value
 *
 * Return post-commit fence pointer pointer or NULL in case of error.
 */
struct sde_post_commit_fence *sde_post_commit_fence_create(
		struct sde_generic_fence_context *ctx,
		int64_t *val);

/**
 * sde_trigger_post_commit_fence - trigger the fill data callback
 * of all post-commit fences which meet the conditions, and set
 * fence flag to signaled state
 *
 * @ctx: the fence context pointer
 */
void sde_trigger_post_commit_fence(
		struct sde_generic_fence_context *ctx);

#endif /* _SDE_POST_COMMIT_FENCE_H_ */

