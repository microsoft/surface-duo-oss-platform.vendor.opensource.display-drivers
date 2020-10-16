/* Copyright (c) 2020 The Linux Foundation. All rights reserved.
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

#ifndef __MSM_HDCP_H_
#define __MSM_HDCP_H_

#include <linux/msm_hdcp.h>

#if IS_ENABLED(CONFIG_HDCP_QSEECOM)
void msm_hdcp_notify_status(struct device *dev, int state,
		int version);
#else
static inline void msm_hdcp_notify_status(struct device *dev, int state,
		int version)
{
}
#endif

#endif
