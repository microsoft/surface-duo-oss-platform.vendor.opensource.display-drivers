// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include "sde_fence_post_commit.h"

static bool sde_post_commit_fence_is_signaled(struct dma_fence *fence)
{
	struct sde_generic_fence *generic_fence;
	struct sde_post_commit_fence *post_commit_fence;

	generic_fence = to_sde_generic_fence(fence);
	post_commit_fence = to_sde_post_commit_fence(generic_fence);

	pr_debug("post-commit fence tirgger mask: %x\n",
			post_commit_fence->trigger_mask);

	return (post_commit_fence->trigger_mask ==
			post_commit_fence->sub_fence_mask);
}

static void sde_post_commit_fence_release(struct dma_fence *fence)
{
	struct sde_post_commit_fence_context *ctx;
	struct sde_generic_fence *generic_fence;
	struct sde_post_commit_fence *post_commit_fence;
	int i;

	generic_fence = to_sde_generic_fence(fence);
	post_commit_fence = to_sde_post_commit_fence(generic_fence);
	ctx = post_commit_fence->ctx;

	for (i = 0; i < SDE_SUB_FENCE_MAX; i++) {
		if (!(post_commit_fence->sub_fence_mask | BIT(i)))
			continue;

		if (!ctx->sub_fence_ctx[i]
			|| !ctx->sub_fence_ctx[i]->ops
			|| !ctx->sub_fence_ctx[i]->ops->release)
			continue;

		if (ctx->sub_fence_ctx[i]->ops->release)
			ctx->sub_fence_ctx[i]->ops->release(
					post_commit_fence->sub_fence[i]);
	}

	kfree(post_commit_fence);
}

static const struct sde_generic_fence_ops fence_ops = {
	.is_signaled = sde_post_commit_fence_is_signaled,
	.release = sde_post_commit_fence_release,
};

int sde_post_commit_fence_ctx_init(
		struct sde_post_commit_fence_context *ctx,
		const char *name,
		unsigned int *done_count)
{
	if (!ctx || !name || !done_count) {
		pr_err("%s: invalid args\n", __func__);
		return -EINVAL;
	}

	if (sde_fence_helper_ctx_init(&ctx->base, name))
		return -EINVAL;

	ctx->done_count = done_count;

	return 0;
}
EXPORT_SYMBOL(sde_post_commit_fence_ctx_init);

int sde_post_commit_add_sub_fence_ctx(
		struct sde_post_commit_fence_context *ctx,
		struct sde_sub_fence_context *sub_fence_ctx)
{
	if (!ctx || !sub_fence_ctx) {
		pr_err("%s: invalid args\n", __func__);
		return -EINVAL;
	}

	ctx->sub_fence_ctx[sub_fence_ctx->type] = sub_fence_ctx;

	return 0;
}
EXPORT_SYMBOL(sde_post_commit_add_sub_fence_ctx);

void sde_post_commit_fence_create(
		struct sde_post_commit_fence_context *ctx,
		uint32_t post_commit_fence_mask,
		unsigned int val)
{
	struct sde_post_commit_fence *post_commit_fence;
	int ret;
	int i;

	if (!ctx || !post_commit_fence_mask)
		return;

	post_commit_fence = kzalloc(sizeof(*post_commit_fence), GFP_KERNEL);
	if (!post_commit_fence)
		return;

	ret = sde_fence_helper_init(&ctx->base,
			&post_commit_fence->base, &fence_ops, val);
	if (ret) {
		pr_err("create post-commit fence failed\n");
		kfree(post_commit_fence);
		return;
	}

	post_commit_fence->ctx = ctx;
	post_commit_fence->sub_fence_mask = post_commit_fence_mask;

	for (i = 0; i < SDE_SUB_FENCE_MAX; i++) {
		if (!(post_commit_fence->sub_fence_mask & BIT(i)))
			continue;

		if (!ctx->sub_fence_ctx[i]
			|| !ctx->sub_fence_ctx[i]->ops
			|| !ctx->sub_fence_ctx[i]->ops->prepare)
			continue;

		if (ctx->sub_fence_ctx[i]->ops->prepare	&&
				ctx->sub_fence_ctx[i]->ops->prepare(
				post_commit_fence)) {
			pr_err("prepare sub-fence(%d) failed\n", i);
			return;
		}
	}
}
EXPORT_SYMBOL(sde_post_commit_fence_create);

int sde_post_commit_add_sub_fence(
		struct sde_sub_fence *sub_fence)
{
	struct sde_fence_file *fence_file;
	int fd;

	if (!sub_fence) {
		pr_err("%s: invalid sub-fence\n", __func__);
		return -EINVAL;
	}

	fd = get_unused_fd_flags(0);
	if (fd < 0) {
		pr_err("%s: failed to get_unused_fd_flags()\n", __func__);
		return fd;
	}

	fence_file = sde_fence_file_create(
			&sub_fence->parent->base.base,
			sub_fence->file_ops);
	if (!fence_file) {
		pr_err("%s: sde_fence_file allocation failed\n", __func__);
		put_unused_fd(fd);
		return -ENOMEM;
	}

	fd_install(fd, fence_file->file);
	sub_fence->fd = fd;
	sub_fence->file = fence_file->file;
	sub_fence->parent->sub_fence[sub_fence->type] = sub_fence;

	return 0;
}
EXPORT_SYMBOL(sde_post_commit_add_sub_fence);

int sde_post_commit_fence_update(
		struct sde_post_commit_fence_context *ctx)
{
	struct sde_generic_fence *generic_fence;
	struct sde_post_commit_fence *post_commit_fence;
	unsigned long flags;
	int i;

	if (!ctx) {
		pr_err("%s: invalid context\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&ctx->base.list_lock, flags);
	if (list_empty(&ctx->base.fence_list_head)) {
		spin_unlock_irqrestore(&ctx->base.list_lock, flags);
		pr_err("generic fence list is empty!\n");
		return -EINVAL;
	}

	generic_fence = list_last_entry(&ctx->base.fence_list_head,
			struct sde_generic_fence, fence_list);
	post_commit_fence = to_sde_post_commit_fence(generic_fence);
	spin_unlock_irqrestore(&ctx->base.list_lock, flags);

	for (i = 0; i < SDE_SUB_FENCE_MAX; i++) {
		if (!(post_commit_fence->sub_fence_mask & BIT(i)))
			continue;

		if (!ctx->sub_fence_ctx[i]
			|| !ctx->sub_fence_ctx[i]->ops
			|| !ctx->sub_fence_ctx[i]->ops->update)
			continue;

		if (!ctx->sub_fence_ctx[i]->ops->update
			|| !ctx->sub_fence_ctx[i]->ops->update(
					post_commit_fence->sub_fence[i])) {
			pr_err("update sub_fence(%d) failed\n", i);
			return -EINVAL;
		}
	}

	return 0;
}
EXPORT_SYMBOL(sde_post_commit_fence_update);

static struct sde_post_commit_fence *sde_post_commit_get_current_locked(
		struct sde_post_commit_fence_context *ctx)
{
	struct sde_generic_fence *f;
	struct sde_post_commit_fence *post_commit_fence = NULL;
	unsigned int done_count;
	unsigned long flags;

	if (!ctx) {
		pr_err("%s: invalid context\n", __func__);
		return NULL;
	}

	done_count = *ctx->done_count;

	spin_lock_irqsave(&ctx->base.list_lock, flags);
	list_for_each_entry(f, &ctx->base.fence_list_head, fence_list) {
		if (f->base.seqno == done_count) {
			post_commit_fence = to_sde_post_commit_fence(f);
			break;
		}
	}
	spin_unlock_irqrestore(&ctx->base.list_lock, flags);

	return post_commit_fence;
}

static void sde_post_commit_trigger_fence(
		struct sde_post_commit_fence_context *ctx)
{
	struct sde_post_commit_fence *post_commit_fence;
	int i;

	post_commit_fence = sde_post_commit_get_current_locked(ctx);
	if (!post_commit_fence)
		return;

	for (i = 0; i < SDE_SUB_FENCE_MAX; i++) {
		if ((post_commit_fence->trigger_mask & BIT(i))
			|| !(post_commit_fence->sub_fence_mask & BIT(i)))
			continue;

		if (!ctx->sub_fence_ctx[i]
			|| !ctx->sub_fence_ctx[i]->ops
			|| !ctx->sub_fence_ctx[i]->ops->fill_data)
			continue;

		ctx->sub_fence_ctx[i]->ops->fill_data(
				post_commit_fence->sub_fence[i], true);
		post_commit_fence->trigger_mask |= BIT(i);
	}
}

void sde_post_commit_signal_fence(struct sde_post_commit_fence_context *ctx)
{
	if (!ctx)
		return;

	sde_post_commit_trigger_fence(ctx);

	sde_fence_helper_signal(&ctx->base);
}
EXPORT_SYMBOL(sde_post_commit_signal_fence);

void sde_post_commit_signal_sub_fence(
		struct sde_post_commit_fence_context *ctx,
		enum sde_sub_fence_type type)
{
	struct sde_post_commit_fence *post_commit_fence;

	if (!ctx || !ctx->sub_fence_ctx[type]
		|| !ctx->sub_fence_ctx[type]->ops)
		return;

	post_commit_fence = sde_post_commit_get_current_locked(ctx);
	if (!post_commit_fence)
		return;

	if (!(post_commit_fence->sub_fence_mask | BIT(type)))
		return;

	if (ctx->sub_fence_ctx[type]->ops->fill_data
		&& ctx->sub_fence_ctx[type]->ops->fill_data(
		post_commit_fence->sub_fence[type], false)) {
		post_commit_fence->trigger_mask |= BIT(type);
		sde_fence_helper_signal(&ctx->base);
	}
}
EXPORT_SYMBOL(sde_post_commit_signal_sub_fence);

