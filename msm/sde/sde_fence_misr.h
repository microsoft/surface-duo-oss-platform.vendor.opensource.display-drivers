/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_MISR_FENCE_H_
#define _SDE_MISR_FENCE_H_

#include "sde_fence_post_commit.h"
#include "sde_fence_file.h"

/**
 * sde_misr_fence_create - create a misr fence
 * @fence: the post-commit fence pointer
 *
 * Returns 0 if success, other errno values if failed.
 */
int sde_misr_fence_create(struct sde_post_commit_fence *fence);

#endif /* _SDE_MISR_FENCE_H_ */

