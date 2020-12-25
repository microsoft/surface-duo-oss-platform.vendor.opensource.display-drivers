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

/**
 * struct sde_fence_file_ops - operations structure of fence file
 * @read: the callback pointer of file operation read
 */
struct sde_fence_file_ops {
	uint32_t (*read)(struct dma_fence *fence, void __user *usr_ptr,
			size_t len);
};

/**
 * struct sde_fence_file - fence file structure
 * @file: file representing fence
 * @wq: wait queue for fence signaling
 * @flags: fence callback flag
 * @fence: dma fence pointer
 * @cb: dma fence callback
 * @ops: the operation pointer of fence file
 */
struct sde_fence_file {
	struct file *file;
	wait_queue_head_t wq;
	unsigned long flags;
	struct dma_fence *fence;
	struct dma_fence_cb cb;
	const struct sde_fence_file_ops *ops;
};

/**
 * sde_fence_file_create() - creates a sde fence file
 * @fence: fence to add to the sde_fence
 * @ops: the callback pointer of fence file operations
 *
 * Creates a sde_fence_file contain @fence. This function acquires and
 * additional reference of @fence for the newly-created &sde_fence_file,
 * if it succeeds. The sde_fence_file can be released with
 * fput(sde_fence_file->file). Returns the sde_fence_file or NULL in
 * case of error.
 */
struct sde_fence_file *sde_fence_file_create(
		struct dma_fence *fence,
		const struct sde_fence_file_ops *ops);

#endif /* _SDE_FENCE_FILE_H */

