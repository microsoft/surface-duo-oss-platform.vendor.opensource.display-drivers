/* Copyright (c) 2020 The Linux Foundation. All rights reserved.
 *
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/anon_inodes.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include "sde_fence_file.h"

#define POLL_ENABLED 0

static ssize_t sde_file_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *pos)
{
	struct sde_fence_file *fence_file = file->private_data;
	unsigned int len = 0;

	if (fence_file->ops->read)
		len = fence_file->ops->read(fence_file->fence,
				user_buf, count);

	return len;
}

static void sde_file_check_cb_func(struct dma_fence *fence,
		struct dma_fence_cb *cb)
{
	struct sde_fence_file *fence_file;

	fence_file = container_of(cb, struct sde_fence_file, cb);

	wake_up_all(&fence_file->wq);
}

static unsigned int sde_file_poll(struct file *file, poll_table *wait)
{
	struct sde_fence_file *fence_file = file->private_data;

	poll_wait(file, &fence_file->wq, wait);

	if (list_empty(&fence_file->cb.node) &&
		!test_and_set_bit(POLL_ENABLED, &fence_file->flags))
		if (dma_fence_add_callback(fence_file->fence,
			&fence_file->cb, sde_file_check_cb_func) < 0)
			wake_up_all(&fence_file->wq);

	return dma_fence_is_signaled(fence_file->fence) ? POLLIN : 0;
}

static int sde_file_release(struct inode *inode, struct file *file)
{
	struct sde_fence_file *fence_file = file->private_data;

	if (test_bit(POLL_ENABLED, &fence_file->flags))
		dma_fence_remove_callback(fence_file->fence,
				&fence_file->cb);

	dma_fence_put(fence_file->fence);
	kfree(fence_file);

	return 0;
}

static const struct file_operations fence_file_fops = {
	.read = sde_file_read,
	.poll = sde_file_poll,
	.release = sde_file_release,
};

struct sde_fence_file *sde_fence_file_create(
		struct dma_fence *fence,
		const struct sde_fence_file_ops *ops)
{
	struct sde_fence_file *fence_file;

	fence_file = kzalloc(sizeof(*fence_file), GFP_KERNEL);
	if (!fence_file)
		return NULL;

	fence_file->file = anon_inode_getfile("fence_file",
			&fence_file_fops, fence_file, 0);
	if (IS_ERR(fence_file->file)) {
		kfree(fence_file);
		return NULL;
	}

	init_waitqueue_head(&fence_file->wq);
	INIT_LIST_HEAD(&fence_file->cb.node);
	fence_file->fence = dma_fence_get(fence);
	fence_file->ops = ops;

	return fence_file;
}
EXPORT_SYMBOL(sde_fence_file_create);

