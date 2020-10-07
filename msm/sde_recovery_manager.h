/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_RECOVERY_MANAGER_INTERNAL_H__
#define __SDE_RECOVERY_MANAGER_INTERNAL_H__

#include <linux/sde_recovery_manager.h>

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
