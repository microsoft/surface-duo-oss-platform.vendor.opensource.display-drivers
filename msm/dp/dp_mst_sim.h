/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_MST_SIM_H_
#define _DP_MST_SIM_H_

#include <linux/msm_dp_aux_bridge.h>
#include <linux/msm_dp_mst_sim_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_modes.h>

enum dp_sim_mode_type {
	DP_SIM_MODE_EDID       = 0x00000001,
	DP_SIM_MODE_DPCD_READ  = 0x00000002,
	DP_SIM_MODE_DPCD_WRITE = 0x00000004,
	DP_SIM_MODE_LINK_TRAIN = 0x00000008,
	DP_SIM_MODE_MST        = 0x00000010,
	DP_SIM_MODE_ALL        = 0x0000001F,
};

int dp_sim_create_bridge(struct device *dev,
		struct msm_dp_aux_bridge **bridge);

int dp_sim_destroy_bridge(struct msm_dp_aux_bridge *bridge);

int dp_sim_set_sim_mode(struct msm_dp_aux_bridge *bridge, u32 sim_mode);

int dp_sim_update_port_num(struct msm_dp_aux_bridge *bridge, u32 port_num);

int dp_sim_update_port_status(struct msm_dp_aux_bridge *bridge,
		int port, enum drm_connector_status status);

int dp_sim_update_port_edid(struct msm_dp_aux_bridge *bridge,
		int port, const u8 *edid, u32 size);

int dp_sim_write_dpcd_reg(struct msm_dp_aux_bridge *bridge,
		const u8 *dpcd, u32 size, u32 offset);

int dp_sim_read_dpcd_reg(struct msm_dp_aux_bridge *bridge,
		u8 *dpcd, u32 size, u32 offset);

#endif /* _DP_MST_SIM_H_ */
