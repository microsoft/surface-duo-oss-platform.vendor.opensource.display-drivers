/* Copyright (c) 2021 The Linux Foundation. All rights reserved.
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
struct msm_hdcp_status {
	int state;
	int version;
	u8 min_enc_level;
};
#if IS_ENABLED(CONFIG_HDCP_QSEECOM)
void msm_hdcp_notify_status(struct device *dev,
		struct msm_hdcp_status *status);
#else
static inline void msm_hdcp_notify_status(struct device *dev,
		struct msm_hdcp_status *status)
{
}
#endif

#endif
