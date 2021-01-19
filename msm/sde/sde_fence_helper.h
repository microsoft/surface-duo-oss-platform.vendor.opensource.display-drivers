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
#ifndef _SDE_FENCE_HELPER_H_
#define _SDE_FENCE_HELPER_H_

#include <linux/spinlock.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/dma-fence.h>
#include <linux/file.h>

#define SDE_GENERIC_FENCE_NAME_SIZE	64

/**
 * struct sde_generic_fence_context - sde generic fence context structure
 * @kref: kref counter on context
 * @lock: spinlock for sde fence context protection
 * @list_lock: spinlock for sde fence list protection
 * @context: fence context
 * @fence_list_head: fence list to hold all the fence created on this context
 * @name: name of fence context
 * @destroy: destroy the private data of generic fence context
 * @private_data: private data for fence context instance
 */
struct sde_generic_fence_context {
	struct kref kref;
	spinlock_t lock;
	spinlock_t list_lock;
	u64 context;
	struct list_head fence_list_head;
	char name[SDE_GENERIC_FENCE_NAME_SIZE];
	void (*destroy)(struct sde_generic_fence_context *);
	void *private_data;
};

/**
 * struct sde_generic_fence_ops - custom operations of sde generic fence
 * @is_signaled: check the signaled condition is met or not
 * @create_file: create the fence file
 * @release: release a generic fence instance
 */
struct sde_generic_fence_ops {
	bool (*is_signaled)(struct dma_fence *);
	struct file *(*create_file)(struct dma_fence *);
	void (*release)(struct dma_fence *);
};

/**
 * struct sde_generic_fence - sde generic fence structure
 * Output fields:
 * @base: dma fence object of this sde fence
 * @file: file attached to this fence
 * @fence_list: list to associated this fence on timeline/context
 *
 * Input fields:
 * @ctx: fence context pointer
 * @name: the name of this sde fence object
 *        format is: sde_generic_fence:ctx_name:commit_count
 * @ops: operations of fence
 */
struct sde_generic_fence {
	struct dma_fence base;
	struct file *file;
	struct list_head fence_list;
	struct sde_generic_fence_context *ctx;
	char name[SDE_GENERIC_FENCE_NAME_SIZE];
	const struct sde_generic_fence_ops *ops;
};

static inline struct sde_generic_fence *to_sde_generic_fence(
		struct dma_fence *fence)
{
	return container_of(fence, struct sde_generic_fence, base);
}

/**
 * sde_generic_fence_ctx_init() - init generic fence context
 * @name: the generic fence context name pointer
 * @destroy: context destroy function pointer
 * @private_data: the private data of fence context
 *
 * Return generic fence context pointer or ERR_PTR in case of error.
 */
struct sde_generic_fence_context *sde_generic_fence_ctx_init(
		const char *name,
		void (*destroy)(struct sde_generic_fence_context *),
		void *private_data);

/**
 * sde_generic_fence_ctx_deinit() - deinit generic fence context
 * @ctx: the generic fence context pointer
 */
void sde_generic_fence_ctx_deinit(
		struct sde_generic_fence_context *ctx);

/**
 * sde_generic_fence_init() - init a fence instance
 * @ctx: the generic fence context pointer
 * @fence: the generic fence pointer should be initialized
 * @ops: customized operations of generic fence
 * @val: the sequence number of dma-fence for initialization
 *
 * Return available fence fd or errno in case of error.
 */
int sde_generic_fence_init(
		struct sde_generic_fence_context *ctx,
		struct sde_generic_fence *generic_fence,
		const struct sde_generic_fence_ops *ops,
		unsigned int val);

/**
 * sde_generic_fence_signal() - the signal method for signaling fence
 * @ctx: the generic fence context pointer
 */
void sde_generic_fence_signal(
		struct sde_generic_fence_context *ctx);

#endif /* _SDE_FENCE_HELPER_H_ */

