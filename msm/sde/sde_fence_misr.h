// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_MISR_FENCE_H_
#define _SDE_MISR_FENCE_H_

#include "sde_crtc.h"

/**
 * sde_misr_fence_ctx_init - initialize the sub-fence context of misr fence
 * @sde_crtc: the sde_crtc pointer
 */
void sde_misr_fence_ctx_init(struct sde_crtc *sde_crtc);

#endif /* _SDE_MISR_FENCE_H_ */

