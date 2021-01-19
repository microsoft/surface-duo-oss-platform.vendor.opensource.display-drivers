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

static const char *sde_generic_fence_get_driver_name(
		struct dma_fence *fence)
{
	struct sde_generic_fence *f = to_sde_generic_fence(fence);

	return f->name;
}

static const char *sde_generic_fence_get_timeline_name(
		struct dma_fence *fence)
{
	struct sde_generic_fence *f = to_sde_generic_fence(fence);

	return f->ctx->name;
}

static bool sde_generic_fence_enable_signaling(
		struct dma_fence *fence)
{
	return true;
}

static bool sde_generic_fence_signaled(struct dma_fence *fence)
{
	struct sde_generic_fence *f = to_sde_generic_fence(fence);

	return f->ops->is_signaled(fence);
}

static void sde_generic_fence_ctx_destroy(struct kref *kref)
{
	struct sde_generic_fence_context *ctx;

	if (!kref) {
		pr_err("ctx_destroy: received invalid kref\n");
		return;
	}

	ctx = container_of(kref, struct sde_generic_fence_context, kref);

	if (ctx->destroy)
		ctx->destroy(ctx);
	kfree(ctx);
}

static void sde_generic_fence_release(struct dma_fence *fence)
{
	struct sde_generic_fence_context *ctx;
	struct sde_generic_fence *f;

	if (fence) {
		f = to_sde_generic_fence(fence);
		ctx = f->ctx;

		if (f->ops->release)
			f->ops->release(fence);

		kref_put(&ctx->kref, sde_generic_fence_ctx_destroy);
	}
}

const static struct dma_fence_ops sde_fence_dma_fence_ops = {
	.get_driver_name = sde_generic_fence_get_driver_name,
	.get_timeline_name = sde_generic_fence_get_timeline_name,
	.enable_signaling = sde_generic_fence_enable_signaling,
	.signaled = sde_generic_fence_signaled,
	.wait = dma_fence_default_wait,
	.release = sde_generic_fence_release,
};

struct sde_generic_fence_context *sde_generic_fence_ctx_init(
		const char *name,
		void (*destroy)(struct sde_generic_fence_context *),
		void *private_data)
{
	struct sde_generic_fence_context *ctx;

	if (!name) {
		pr_err("ctx_init: invalid name\n");
		return ERR_PTR(-EINVAL);
	}

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return ERR_PTR(-ENOMEM);

	strlcpy(ctx->name, name, ARRAY_SIZE(ctx->name));
	kref_init(&ctx->kref);
	ctx->context = dma_fence_context_alloc(1);
	ctx->destroy = destroy;
	ctx->private_data = private_data;

	spin_lock_init(&ctx->lock);
	spin_lock_init(&ctx->list_lock);
	INIT_LIST_HEAD(&ctx->fence_list_head);

	return ctx;
}
EXPORT_SYMBOL(sde_generic_fence_ctx_init);

void sde_generic_fence_ctx_deinit(
		struct sde_generic_fence_context *ctx)
{
	if (!ctx) {
		pr_err("ctx_deinit: invalid ctx\n");
		return;
	}

	kref_put(&ctx->kref, sde_generic_fence_ctx_destroy);
}
EXPORT_SYMBOL(sde_generic_fence_ctx_deinit);

int sde_generic_fence_init(
		struct sde_generic_fence_context *ctx,
		struct sde_generic_fence *generic_fence,
		const struct sde_generic_fence_ops *ops,
		unsigned int val)
{
	signed int fd = -EINVAL;
	struct file *file;

	if (!ctx || !generic_fence) {
		pr_err("%s: invalid args\n", __func__);
		return -EINVAL;
	}

	if (!ops->is_signaled || !ops->create_file) {
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

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		pr_err("%s: failed to get_unused_fd_flags()\n",
				generic_fence->name);
		goto error;
	}

	file = generic_fence->ops->create_file(&generic_fence->base);
	if (!file)
		goto file_error;

	fd_install(fd, file);
	generic_fence->file = file;
	kref_get(&ctx->kref);

	spin_lock(&ctx->list_lock);
	list_add_tail(&generic_fence->fence_list, &ctx->fence_list_head);
	spin_unlock(&ctx->list_lock);

	return fd;

file_error:
	pr_err("%s: file allocation failed\n", generic_fence->name);
	put_unused_fd(fd);
	fd = -ENOMEM;
error:
	dma_fence_put(&generic_fence->base);
	kfree(generic_fence);
	return fd;
}
EXPORT_SYMBOL(sde_generic_fence_init);

void sde_generic_fence_signal(
		struct sde_generic_fence_context *ctx)
{
	struct sde_generic_fence *f, *next;
	struct list_head local_list_head;
	bool is_signaled = false;
	unsigned long flags;

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
EXPORT_SYMBOL(sde_generic_fence_signal);

