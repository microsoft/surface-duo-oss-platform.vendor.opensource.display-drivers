/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SURFACE_DSI_PANEL_H_
#define _SURFACE_DSI_PANEL_H_

#define SURFACE_DSI_ELVSS_LEVELS_MAX	10

/**
 * enum surface_dsi_cmd_set_type  - DSI command set type
 * @SURFACE_DSI_CMD_AOD_ON                         Enable AOD mode
 * @SURFACE_DSI_CMD_AOD_OFF                        Disable AOD mode
 * @SURFACE_DSI_CMD_SWITCH_RR_60                   Set 60 HZ
 * @SURFACE_DSI_CMD_SWITCH_RR_90                   Set 90 HZ
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_0                  Elvss level 0
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_1                  Elvss level 1
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_2                  Elvss level 2
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_3                  Elvss level 3
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_4                  Elvss level 4
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_5                  Elvss level 5
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_6                  Elvss level 6
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_7                  Elvss level 7
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_8                  Elvss level 8
 * @SURFACE_DSI_CMD_ELVSS_LEVEL_9                  Elvss level 9
 * @SURFACE_DSI_CMD_SET_MAX
 */
enum surface_dsi_cmd_set_type {
	SURFACE_DSI_CMD_AOD_ON = DSI_CMD_SET_MAX,
	SURFACE_DSI_CMD_AOD_OFF,
	SURFACE_DSI_CMD_SWITCH_RR_60,
	SURFACE_DSI_CMD_SWITCH_RR_90,
	SURFACE_DSI_CMD_ELVSS_LEVEL_0,
	SURFACE_DSI_CMD_ELVSS_LEVEL_1,
	SURFACE_DSI_CMD_ELVSS_LEVEL_2,
	SURFACE_DSI_CMD_ELVSS_LEVEL_3,
	SURFACE_DSI_CMD_ELVSS_LEVEL_4,
	SURFACE_DSI_CMD_ELVSS_LEVEL_5,
	SURFACE_DSI_CMD_ELVSS_LEVEL_6,
	SURFACE_DSI_CMD_ELVSS_LEVEL_7,
	SURFACE_DSI_CMD_ELVSS_LEVEL_8,
	SURFACE_DSI_CMD_ELVSS_LEVEL_9,
	SURFACE_DSI_CMD_SET_MAX
};
#endif /* _SURFACE_DSI_DEFS_H_ */