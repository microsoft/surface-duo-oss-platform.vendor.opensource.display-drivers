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
#include "sde_fence_file.h"

#define POLL_ENABLED 0

static ssize_t sde_file_read(struct file *file,
		char __user *user_buf, size_t count, loff_t *pos)
{
	struct sde_fence_file *fence_file = file->private_data;
	struct sde_sub_fence *cur;
	unsigned int copied_len = 0, copied_count = 0;

	list_for_each_entry(cur, &fence_file->sub_fence_list, node) {
		if (cur->cb->read)
			copied_len = cur->cb->read(cur,
					&user_buf[copied_count],
					count - copied_count);

		copied_count += copied_len;
	}

	return copied_count;
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
	struct sde_sub_fence *cur, *tmp;

	if (test_bit(POLL_ENABLED, &fence_file->flags))
		dma_fence_remove_callback(fence_file->fence,
				&fence_file->cb);

	dma_fence_put(fence_file->fence);

	list_for_each_entry_safe(cur, tmp,
		&fence_file->sub_fence_list, node) {
		list_del_init(&cur->node);
		if (cur->cb->release)
			cur->cb->release(cur);
	}

	kfree(fence_file);

	return 0;
}

static const struct file_operations fence_file_fops = {
	.read = sde_file_read,
	.poll = sde_file_poll,
	.release = sde_file_release,
};

int sde_fence_file_add_sub_fence(struct sde_fence_file *fence_file,
		struct sde_sub_fence *sub_fence)
{
	unsigned long flags;

	if (!sub_fence->cb || !sub_fence->cb->release) {
		pr_err("%s:invalid cb arg\n", __func__);
		return -EINVAL;
	}

	spin_lock_irqsave(&fence_file->list_lock, flags);
	list_add_tail(&sub_fence->node, &fence_file->sub_fence_list);
	spin_unlock_irqrestore(&fence_file->list_lock, flags);

	return 0;
}
EXPORT_SYMBOL(sde_fence_file_add_sub_fence);

void sde_fence_file_trigger(struct sde_fence_file *fence_file)
{
	struct sde_sub_fence *cur;
	unsigned long flags;

	if (!fence_file)
		return;

	spin_lock_irqsave(&fence_file->list_lock, flags);
	list_for_each_entry(cur, &fence_file->sub_fence_list, node)
		if (cur->cb->fill_data)
			cur->cb->fill_data(cur);
	spin_unlock_irqrestore(&fence_file->list_lock, flags);
}
EXPORT_SYMBOL(sde_fence_file_trigger);

struct sde_fence_file *sde_fence_file_create(struct dma_fence *fence)
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
	spin_lock_init(&fence_file->list_lock);
	INIT_LIST_HEAD(&fence_file->sub_fence_list);

	fence_file->fence = dma_fence_get(fence);

	return fence_file;
}
EXPORT_SYMBOL(sde_fence_file_create);

