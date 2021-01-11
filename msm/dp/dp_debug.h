/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_DEBUG_H_
#define _DP_DEBUG_H_

#include "dp_panel.h"
#include "dp_ctrl.h"
#include "dp_link.h"
#include "dp_usbpd.h"
#include "dp_aux.h"
#include "dp_display.h"

/**
 * struct dp_debug
 * @hdcp_wait_sink_sync: used to wait for sink synchronization before HDCP auth
 * @tpg_state: specifies whether tpg feature is enabled
 * @max_pclk_khz: max pclk supported
 * @force_encryption: enable/disable forced encryption for HDCP 2.2
 */
struct dp_debug {
	bool sim_mode;
	bool psm_enabled;
	bool hdcp_disabled;
	bool hdcp_wait_sink_sync;
	bool tpg_state;
	u32 max_pclk_khz;
	bool force_encryption;
	char hdcp_status[SZ_128];
	bool force_bond_mode;
	void (*abort)(struct dp_debug *dp_debug);
};

/**
 * struct dp_debug_in
 * @dev: device instance of the caller
 * @index: device index
 * @panel: instance of panel module
 * @hpd: instance of hpd module
 * @link: instance of link module
 * @aux: instance of aux module
 * @connector: double pointer to display connector
 * @catalog: instance of catalog module
 * @parser: instance of parser module
 * @ctrl: instance od ctrl module
 * @power: instance of  power module
 */
struct dp_debug_in {
	struct device *dev;
	u32 index;
	struct dp_panel *panel;
	struct dp_hpd *hpd;
	struct dp_link *link;
	struct dp_aux *aux;
	struct drm_connector **connector;
	struct dp_catalog *catalog;
	struct dp_parser *parser;
	struct dp_ctrl *ctrl;
	struct dp_power *power;
};

/**
 * dp_debug_get() - configure and get the DisplayPlot debug module data
 *
 * @in: input structure containing data to initialize the debug module
 * return: pointer to allocated debug module data
 *
 * This function sets up the debug module and provides a way
 * for debugfs input to be communicated with existing modules
 */
struct dp_debug *dp_debug_get(struct dp_debug_in *in);

/**
 * dp_debug_put()
 *
 * Cleans up dp_debug instance
 *
 * @dp_debug: instance of dp_debug
 */
void dp_debug_put(struct dp_debug *dp_debug);
#endif /* _DP_DEBUG_H_ */
