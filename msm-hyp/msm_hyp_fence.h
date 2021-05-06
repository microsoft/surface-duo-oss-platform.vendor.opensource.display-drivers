/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_HYP_FENCE_H_
#define _MSM_HYP_FENCE_H_

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>

#define MSM_HYP_FENCE_NAME_SIZE	24

/**
 * struct msm_hyp_fence_context - fence context/timeline structure
 * @commit_count: Number of detected commits since bootup
 * @done_count: Number of completed commits since bootup
 * @ref: kref counter on timeline
 * @lock: spinlock for fence counter protection
 * @list_lock: spinlock for timeline protection
 * @context: fence context
 * @list_head: fence list to hold all the fence created on this context
 * @name: name of fence context/timeline
 */
struct msm_hyp_fence_context {
	unsigned int commit_count;
	unsigned int done_count;
	struct kref kref;
	spinlock_t lock;
	spinlock_t list_lock;
	u64 context;
	struct list_head fence_list_head;
	char name[MSM_HYP_FENCE_NAME_SIZE];
};

#if IS_ENABLED(CONFIG_SYNC_FILE)
void *msm_hyp_sync_get(uint64_t fd);
void msm_hyp_sync_put(void *fence);
signed long msm_hyp_sync_wait(void *fence, long timeout_ms);
struct msm_hyp_fence_context *msm_hyp_fence_init(const char *name);
void msm_hyp_fence_deinit(struct msm_hyp_fence_context *fence);
void msm_hyp_fence_prepare(struct msm_hyp_fence_context *fence);
int msm_hyp_fence_create(struct msm_hyp_fence_context *fence,
		uint64_t __user *user_fd_addr, uint32_t offset);
void msm_hyp_fence_signal(struct msm_hyp_fence_context *fence);
#else
static inline void *msm_hyp_sync_get(uint64_t fd)
{
	return NULL;
}

static inline void msm_hyp_sync_put(void *fence)
{
}

static inline signed long msm_hyp_sync_wait(void *fence, long timeout_ms)
{
	return 0;
}

static inline struct msm_hyp_fence_context *msm_hyp_fence_init(const char *name,
		uint32_t drm_id)
{
	/* do nothing */
	return NULL;
}

static inline void msm_hyp_fence_deinit(struct msm_hyp_fence_context *fence)
{
	/* do nothing */
}

static inline void msm_hyp_fence_signal(struct msm_hyp_fence_context *fence)
{
	/* do nothing */
}

static inline void msm_hyp_fence_prepare(struct msm_hyp_fence_context *fence)
{
	/* do nothing */
}

static inline int msm_hyp_fence_create(struct msm_hyp_fence_context *fence,
		uint64_t __user *user_fd_addr, uint32_t offset)
{
	return 0;
}

#endif

#endif
