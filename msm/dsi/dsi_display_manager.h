// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DSI_DISPLAY_MANAGER_H_
#define _DSI_DISPLAY_MANAGER_H_

#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>

#include "msm_drv.h"
#include "sde_connector.h"
#include "msm_mmu.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "sde_dbg.h"
#include "dsi_parser.h"

struct dsi_display_manager {
	struct list_head display_list;
	struct mutex disp_mgr_mutex;
	bool init;
	bool master_gate_pending;
};

enum dsi_display_mgr_ctrl_type {
	 DSI_DISPLAY_MGR_PHY_PWR = 0,
	 DSI_DISPLAY_MGR_PHY_IDLE,
};

static struct dsi_display_manager disp_mgr;

void dsi_display_manager_register(struct dsi_display *display);
void dsi_display_manager_unregister(struct dsi_display *display);

int dsi_display_mgr_phy_enable(struct dsi_display *display);
int dsi_display_mgr_phy_disable(struct dsi_display *display);
int dsi_display_mgr_phy_idle_on(struct dsi_display *display);
int dsi_display_mgr_phy_idle_off(struct dsi_display *display);
int dsi_display_mgr_config_clk_gating(struct dsi_display *display,
		bool en);
int dsi_display_mgr_get_pll_info(struct dsi_display *display,
		enum dsi_pll_info info);
#endif /* _DSI_DISPLAY_MANAGER_H_ */
