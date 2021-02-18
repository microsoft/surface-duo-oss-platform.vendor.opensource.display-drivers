// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include "msm_drv_hyp.h"

int msm_drm_register_component(struct drm_device *dev,
		struct notifier_block *nb)
{
	struct msm_hyp_drm_private *priv;

	if (!dev)
		return -EINVAL;

	priv = dev->dev_private;

	return blocking_notifier_chain_register(&priv->component_notifier_list,
			nb);
}

int msm_drm_unregister_component(struct drm_device *dev,
		struct notifier_block *nb)
{
	struct msm_hyp_drm_private *priv;

	if (!dev)
		return -EINVAL;

	priv = dev->dev_private;

	return blocking_notifier_chain_unregister(
			&priv->component_notifier_list,	nb);
}

int msm_drm_notify_components(struct drm_device *dev,
		enum msm_component_event event)
{
	struct msm_hyp_drm_private *priv;

	if (!dev)
		return -EINVAL;

	priv = dev->dev_private;

	return blocking_notifier_call_chain(&priv->component_notifier_list,
			event, NULL);
}
