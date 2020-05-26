/*
 * Copyright (c) 2018, 2020-2021, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __SDE_RECOVERY_MANAGER_INTERNAL_H__
#define __SDE_RECOVERY_MANAGER_INTERNAL_H__

#include <soc/qcom/sde_recovery_manager.h>

#if IS_ENABLED(CONFIG_SDE_RECOVERY_MANAGER)

/*
 * Return the custom DRM event handler
 * @dev       DRM device
 * @event     event
 *
 * Return value:
 *   NULL     event not supported
 *   Others   event handler
 */
struct sde_crtc_irq_info *sde_recovery_get_event_handler(
		struct drm_device *dev, u32 event);

/*
 * Initialize the recovery manager for a given DRM device
 * @dev       DRM device
 *
 * Return value:
 *   0        Success
 *   -EINVAL  dev is NULL or not initialized
 *   -ENOMEM  out of memory
 */
int sde_init_recovery_mgr(struct drm_device *dev);

/*
 * De-initialize the recovery manager for a given DRM device
 * @dev       DRM device
 *
 * Return value:
 *   0        Success
 *   -ENOENT  recovery manager hasn't been initialized
 */
int sde_deinit_recovery_mgr(struct drm_device *dev);

#else

static struct sde_crtc_irq_info *sde_recovery_get_event_handler(
		struct drm_device *dev, u32 event)
{
	return NULL;
}
static inline int sde_init_recovery_mgr(struct drm_device *dev)
{
	return 0;
}
static inline int sde_deinit_recovery_mgr(struct drm_device *dev)
{
	return 0;
}

#endif

#endif /* __SDE_RECOVERY_MANAGER_INTERNAL_H__ */
