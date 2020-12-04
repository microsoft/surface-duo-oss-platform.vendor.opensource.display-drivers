// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

