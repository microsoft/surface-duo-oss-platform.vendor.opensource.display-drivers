// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__
#include <linux/sync_file.h>
#include <linux/dma-fence.h>
#include "msm_hyp_fence.h"
#include "msm_drv_hyp.h"

#define TIMELINE_VAL_LENGTH		128

void *msm_hyp_sync_get(uint64_t fd)
{
	/* force signed compare, fdget accepts an int argument */
	return (signed int)fd >= 0 ? sync_file_get_fence(fd) : NULL;
}

void msm_hyp_sync_put(void *fence)
{
	if (fence)
		dma_fence_put(fence);
}

signed long msm_hyp_sync_wait(void *fnc, long timeout_ms)
{
	struct dma_fence *fence = fnc;
	int rc;
	char timeline_str[TIMELINE_VAL_LENGTH];

	if (!fence)
		return -EINVAL;
	else if (dma_fence_is_signaled(fence))
		return timeout_ms ? msecs_to_jiffies(timeout_ms) : 1;

	rc = dma_fence_wait_timeout(fence, true,
				msecs_to_jiffies(timeout_ms));
	if (!rc || (rc == -EINVAL)) {
		if (fence->ops->timeline_value_str)
			fence->ops->timeline_value_str(fence,
					timeline_str, TIMELINE_VAL_LENGTH);

		pr_err("%s:%s seqno:0x%x timeline:%s signaled:0x%x\n",
			fence->ops->get_driver_name(fence),
			fence->ops->get_timeline_name(fence),
			fence->seqno, timeline_str,
			fence->ops->signaled ?
				fence->ops->signaled(fence) : 0xffffffff);
	}

	return rc;
}

/**
 * struct msm_hyp_fence - release/retire fence structure
 * @fence: base fence structure
 * @name: name of each fence- it is fence timeline + commit_count
 * @fence_list: list to associated this fence on timeline/context
 */
struct msm_hyp_fence {
	struct dma_fence base;
	struct msm_hyp_fence_context *ctx;
	char name[MSM_HYP_FENCE_NAME_SIZE];
	struct list_head fence_list;
};

static void msm_hyp_fence_destroy(struct kref *kref)
{
	struct msm_hyp_fence_context *ctx;

	if (!kref) {
		pr_err("received invalid kref\n");
		return;
	}

	ctx = container_of(kref, struct msm_hyp_fence_context, kref);
	kfree(ctx);
}

static inline struct msm_hyp_fence *to_msm_hyp_fence(struct dma_fence *fence)
{
	return container_of(fence, struct msm_hyp_fence, base);
}

static const char *msm_hyp_fence_get_driver_name(struct dma_fence *fence)
{
	struct msm_hyp_fence *f = to_msm_hyp_fence(fence);

	return f->name;
}

static const char *msm_hyp_fence_get_timeline_name(struct dma_fence *fence)
{
	struct msm_hyp_fence *f = to_msm_hyp_fence(fence);

	return f->ctx->name;
}

static bool msm_hyp_fence_enable_signaling(struct dma_fence *fence)
{
	return true;
}

static bool msm_hyp_fence_signaled(struct dma_fence *fence)
{
	struct msm_hyp_fence *f = to_msm_hyp_fence(fence);
	bool status;

	status = (int)(fence->seqno - f->ctx->done_count) <= 0;
	pr_debug("status:%d fence seq:%d and timeline:%d\n",
			status, fence->seqno, f->ctx->done_count);
	return status;
}

static void msm_hyp_fence_release(struct dma_fence *fence)
{
	struct msm_hyp_fence *f;

	if (fence) {
		f = to_msm_hyp_fence(fence);
		kref_put(&f->ctx->kref, msm_hyp_fence_destroy);
		kfree(f);
	}
}

static void msm_hyp_fence_value_str(struct dma_fence *fence, char *str, int size)
{
	if (!fence || !str)
		return;

	snprintf(str, size, "%d", fence->seqno);
}

static void msm_hyp_fence_timeline_value_str(struct dma_fence *fence, char *str,
		int size)
{
	struct msm_hyp_fence *f = to_msm_hyp_fence(fence);

	if (!fence || !f->ctx || !str)
		return;

	snprintf(str, size, "%d", f->ctx->done_count);
}

static struct dma_fence_ops msm_hyp_fence_ops = {
	.get_driver_name = msm_hyp_fence_get_driver_name,
	.get_timeline_name = msm_hyp_fence_get_timeline_name,
	.enable_signaling = msm_hyp_fence_enable_signaling,
	.signaled = msm_hyp_fence_signaled,
	.wait = dma_fence_default_wait,
	.release = msm_hyp_fence_release,
	.fence_value_str = msm_hyp_fence_value_str,
	.timeline_value_str = msm_hyp_fence_timeline_value_str,
};

/**
 * _msm_hyp_fence_create_fd - create fence object and return an fd for it
 * This function is NOT thread-safe.
 * @timeline: Timeline to associate with fence
 * @val: Timeline value at which to signal the fence
 * Return: File descriptor on success, or error code on error
 */
static int _msm_hyp_fence_create_fd(void *fence_ctx, uint32_t val)
{
	struct msm_hyp_fence *msm_hyp_fence;
	struct sync_file *sync_file;
	signed int fd = -EINVAL;
	struct msm_hyp_fence_context *ctx = fence_ctx;

	if (!ctx) {
		pr_err("invalid context\n");
		goto exit;
	}

	msm_hyp_fence = kzalloc(sizeof(*msm_hyp_fence), GFP_KERNEL);
	if (!msm_hyp_fence)
		return -ENOMEM;

	msm_hyp_fence->ctx = fence_ctx;
	snprintf(msm_hyp_fence->name, MSM_HYP_FENCE_NAME_SIZE,
			"msm_hyp:%s:%u",
			msm_hyp_fence->ctx->name, val);
	dma_fence_init(&msm_hyp_fence->base, &msm_hyp_fence_ops, &ctx->lock,
			ctx->context, val);
	kref_get(&ctx->kref);

	/* create fd */
	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		pr_err("failed to get_unused_fd_flags(), %s\n",
				msm_hyp_fence->name);
		dma_fence_put(&msm_hyp_fence->base);
		goto exit;
	}

	/* create fence */
	sync_file = sync_file_create(&msm_hyp_fence->base);
	if (sync_file == NULL) {
		put_unused_fd(fd);
		fd = -EINVAL;
		pr_err("couldn't create fence, %s\n", msm_hyp_fence->name);
		dma_fence_put(&msm_hyp_fence->base);
		goto exit;
	}

	fd_install(fd, sync_file->file);

	spin_lock(&ctx->list_lock);
	list_add_tail(&msm_hyp_fence->fence_list, &ctx->fence_list_head);
	spin_unlock(&ctx->list_lock);

exit:
	return fd;
}

struct msm_hyp_fence_context *msm_hyp_fence_init(const char *name)
{
	struct msm_hyp_fence_context *ctx;

	if (!name) {
		pr_err("invalid argument(s)\n");
		return ERR_PTR(-EINVAL);
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	strlcpy(ctx->name, name, sizeof(ctx->name));
	kref_init(&ctx->kref);
	ctx->context = dma_fence_context_alloc(1);

	spin_lock_init(&ctx->lock);
	spin_lock_init(&ctx->list_lock);
	INIT_LIST_HEAD(&ctx->fence_list_head);

	return ctx;
}

void msm_hyp_fence_deinit(struct msm_hyp_fence_context *ctx)
{
	if (!ctx) {
		pr_err("invalid fence\n");
		return;
	}

	kref_put(&ctx->kref, msm_hyp_fence_destroy);
}

void msm_hyp_fence_prepare(struct msm_hyp_fence_context *ctx)
{
	unsigned long flags;

	if (!ctx) {
		pr_err("invalid argument(s), fence %pK\n", ctx);
	} else {
		spin_lock_irqsave(&ctx->lock, flags);
		++ctx->commit_count;
		spin_unlock_irqrestore(&ctx->lock, flags);
	}
}

static void _msm_hyp_fence_trigger(struct msm_hyp_fence_context *ctx)
{
	struct msm_hyp_fence *fc, *next;
	bool is_signaled = false;
	struct list_head local_list_head;

	INIT_LIST_HEAD(&local_list_head);

	spin_lock(&ctx->list_lock);
	if (list_empty(&ctx->fence_list_head)) {
		pr_debug("nothing to trigger!\n");
		spin_unlock(&ctx->list_lock);
		return;
	}

	list_for_each_entry_safe(fc, next, &ctx->fence_list_head, fence_list)
		list_move(&fc->fence_list, &local_list_head);
	spin_unlock(&ctx->list_lock);

	list_for_each_entry_safe(fc, next, &local_list_head, fence_list) {
		is_signaled = dma_fence_get_status(&fc->base);
		if (is_signaled) {
			list_del_init(&fc->fence_list);
			dma_fence_put(&fc->base);
		} else {
			spin_lock(&ctx->list_lock);
			list_move(&fc->fence_list, &ctx->fence_list_head);
			spin_unlock(&ctx->list_lock);
		}
	}
}

int msm_hyp_fence_create(struct msm_hyp_fence_context *ctx,
		uint64_t __user *user_fd_addr, uint32_t offset)
{
	uint32_t trigger_value;
	int fd, rc = -EINVAL;
	unsigned long flags;

	if (!ctx || !user_fd_addr) {
		pr_err("invalid argument(s), fence %d, fd_addr %d\n",
				ctx != NULL, user_fd_addr != NULL);
		return rc;
	}

	spin_lock_irqsave(&ctx->lock, flags);
	trigger_value = ctx->commit_count + offset;
	spin_unlock_irqrestore(&ctx->lock, flags);

	fd = _msm_hyp_fence_create_fd(ctx, trigger_value);

	copy_to_user(user_fd_addr, &fd, sizeof(uint64_t));

	pr_debug("fence_create::fd:%d trigger:%d commit:%d\n",
			fd, trigger_value, ctx->commit_count);

	if (fd >= 0) {
		rc = 0;
		_msm_hyp_fence_trigger(ctx);
	} else {
		rc = fd;
	}

	return rc;
}

void msm_hyp_fence_signal(struct msm_hyp_fence_context *ctx)
{
	unsigned long flags;

	if (!ctx) {
		pr_err("invalid ctx, %pK\n", ctx);
		return;
	}

	spin_lock_irqsave(&ctx->lock, flags);
	if ((int)(ctx->done_count - ctx->commit_count) < 0) {
		++ctx->done_count;
		pr_debug("fence_signal:done count:%d commit count:%d\n",
				ctx->done_count, ctx->commit_count);
	}
	spin_unlock_irqrestore(&ctx->lock, flags);

	_msm_hyp_fence_trigger(ctx);
}
