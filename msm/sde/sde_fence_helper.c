/* Copyright (c) 2016-2018, 2020-2021, The Linux Foundation. All rights reserved.
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

#include "sde_fence_helper.h"

static const char *sde_fence_helper_get_driver_name(
		struct dma_fence *fence)
{
	struct sde_generic_fence *f = to_sde_generic_fence(fence);

	return f->name;
}

static const char *sde_fence_helper_get_timeline_name(
		struct dma_fence *fence)
{
	struct sde_generic_fence *f = to_sde_generic_fence(fence);

	return f->ctx->name;
}

static bool sde_fence_helper_enable_signaling(
		struct dma_fence *fence)
{
	return true;
}

static bool sde_fence_helper_signaled(struct dma_fence *fence)
{
	struct sde_generic_fence *f = to_sde_generic_fence(fence);

	return f->ops->is_signaled(fence);
}

static void sde_fence_helper_release(struct dma_fence *fence)
{
	struct sde_generic_fence_context *ctx;
	struct sde_generic_fence *f;

	if (fence) {
		f = to_sde_generic_fence(fence);
		ctx = f->ctx;

		if (f->ops->release)
			f->ops->release(fence);
	}
}

const static struct dma_fence_ops sde_fence_dma_fence_ops = {
	.get_driver_name = sde_fence_helper_get_driver_name,
	.get_timeline_name = sde_fence_helper_get_timeline_name,
	.enable_signaling = sde_fence_helper_enable_signaling,
	.signaled = sde_fence_helper_signaled,
	.wait = dma_fence_default_wait,
	.release = sde_fence_helper_release,
};

int sde_fence_helper_ctx_init(
		struct sde_generic_fence_context *ctx,
		const char *name)
{
	if (!ctx || !name) {
		pr_err("%s: invalid args\n", __func__);
		return -EINVAL;
	}

	strlcpy(ctx->name, name, ARRAY_SIZE(ctx->name));
	ctx->context = dma_fence_context_alloc(1);
	spin_lock_init(&ctx->lock);
	spin_lock_init(&ctx->list_lock);
	INIT_LIST_HEAD(&ctx->fence_list_head);

	return 0;
}
EXPORT_SYMBOL(sde_fence_helper_ctx_init);

int sde_fence_helper_init(
		struct sde_generic_fence_context *ctx,
		struct sde_generic_fence *generic_fence,
		const struct sde_generic_fence_ops *ops,
		unsigned int val)
{

	if (!ctx || !generic_fence) {
		pr_err("%s: invalid args\n", __func__);
		return -EINVAL;
	}

	if (!ops->is_signaled) {
		pr_err("%s: invalid ops\n", __func__);
		return -EINVAL;
	}

	generic_fence->ctx = ctx;
	generic_fence->ops = ops;
	snprintf(generic_fence->name, SDE_GENERIC_FENCE_NAME_SIZE,
			"sde_generic_fence:%s:%u",
			generic_fence->ctx->name, val);
	dma_fence_init(&generic_fence->base, &sde_fence_dma_fence_ops,
			&ctx->lock, ctx->context, val);

	spin_lock(&ctx->list_lock);
	list_add_tail(&generic_fence->fence_list, &ctx->fence_list_head);
	spin_unlock(&ctx->list_lock);

	return 0;
}
EXPORT_SYMBOL(sde_fence_helper_init);

void sde_fence_helper_signal(
		struct sde_generic_fence_context *ctx)
{
	struct sde_generic_fence *f, *next;
	struct list_head local_list_head;
	bool is_signaled = false;
	unsigned long flags;

	if (!ctx)
		return;

	INIT_LIST_HEAD(&local_list_head);

	spin_lock(&ctx->list_lock);
	if (list_empty(&ctx->fence_list_head)) {
		pr_debug("nothing to trigger!\n");
		spin_unlock(&ctx->list_lock);
		return;
	}

	list_for_each_entry_safe(f, next, &ctx->fence_list_head, fence_list)
		list_move(&f->fence_list, &local_list_head);
	spin_unlock(&ctx->list_lock);

	list_for_each_entry_safe(f, next, &local_list_head, fence_list) {
		spin_lock_irqsave(&ctx->lock, flags);
		is_signaled = dma_fence_is_signaled_locked(&f->base);
		spin_unlock_irqrestore(&ctx->lock, flags);

		if (is_signaled) {
			list_del_init(&f->fence_list);
			dma_fence_put(&f->base);
		} else {
			spin_lock(&ctx->list_lock);
			list_move(&f->fence_list,
				&ctx->fence_list_head);
			spin_unlock(&ctx->list_lock);
		}
	}
}
EXPORT_SYMBOL(sde_fence_helper_signal);

