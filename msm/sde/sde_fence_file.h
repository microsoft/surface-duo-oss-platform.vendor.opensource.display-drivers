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

#ifndef _SDE_FENCE_FILE_H
#define _SDE_FENCE_FILE_H

#include <linux/types.h>
#include <linux/dma-fence.h>
#include <linux/file.h>
#include <linux/poll.h>

struct sde_sub_fence;

/**
 * struct sde_sub_fence_cb - sub-fence callback structure
 * @read: the function pointer for read
 * @fill_data: the function pointer for filling read data
 * @release: the function pointer for release sub-fence
 */
struct sde_sub_fence_cb {
	uint32_t (*read)(struct sde_sub_fence *,
			void __user *usr_ptr, size_t len);
	void (*fill_data)(struct sde_sub_fence *);
	void (*release)(struct sde_sub_fence *);
};

/**
 * struct sde_sub_fence - sub-fence structure
 * @cb: sub-fence callback pointer
 * @node: used by sde_sub_fence_add_callback to append this struct
 *        to sde_fence_file::cb_list
 */
struct sde_sub_fence {
	const struct sde_sub_fence_cb *cb;
	struct list_head node;
};

/**
 * struct sde_fence_file - fence file structure
 * @file: file representing fence
 * @wq: wait queue for fence signaling
 * @flags: fence callback flag
 * @fence: dma fence pointer
 * @cb: dma fence callback
 * @list_lock: spin lock for fence file list protection
 * @sub_fence_list: sub-fence list of this file
 */
struct sde_fence_file {
	struct file *file;
	wait_queue_head_t wq;
	unsigned long flags;
	struct dma_fence *fence;
	struct dma_fence_cb cb;
	spinlock_t list_lock;
	struct list_head sub_fence_list;
};

/**
 * sde_fence_file_add_sub_fence - add a sub-fence to this fence file
 * @fence_file: fence file to be added callback
 * @sub_fence: sub_fence pointer to be added
 *
 * Return errno or zero.
 */
int sde_fence_file_add_sub_fence(struct sde_fence_file *fence_file,
		struct sde_sub_fence *sub_fence);

/**
 * sde_fence_file_trigger - trigger all fill_data callback of this fence_file
 * @fence_file: fence file pointer
 */
void sde_fence_file_trigger(struct sde_fence_file *fence_file);

/**
 * sde_fence_file_create() - creates a sde fence file
 * @fence: fence to add to the sde_fence
 *
 * Creates a sde_fence_file contain @fence. This function acquires and
 * additional reference of @fence for the newly-created &sde_fence_file,
 * if it succeeds. The sde_fence_file can be released with
 * fput(sde_fence_file->file). Returns the sde_fence_file or NULL in
 * case of error.
 */
struct sde_fence_file *sde_fence_file_create(struct dma_fence *fence);

#endif /* _SDE_FENCE_FILE_H */

