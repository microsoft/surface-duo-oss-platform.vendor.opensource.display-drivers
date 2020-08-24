/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_POWER_H_
#define _DP_POWER_H_

#include "dp_parser.h"
#include "sde_power_handle.h"

/**
 * sruct dp_power - DisplayPort's power related data
 *
 * @sim_mode: simulation mode enable flag
 * @init: initializes the regulators/core clocks/GPIOs/pinctrl
 * @deinit: turns off the regulators/core clocks/GPIOs/pinctrl
 * @clk_enable: enable/disable the DP clocks
 * @set_pixel_clk_parent: set the parent of DP pixel clock
 */
struct dp_power {
	bool sim_mode;
	struct drm_device *drm_dev;
	struct sde_power_handle *phandle;
	int (*init)(struct dp_power *power, bool flip);
	int (*deinit)(struct dp_power *power);
	int (*clk_enable)(struct dp_power *power, enum dp_pm_type pm_type,
				bool enable);
	int (*set_pixel_clk_parent)(struct dp_power *power, u32 stream_id,
				enum dp_phy_bond_mode bond_mode, bool enable);
	int (*power_client_init)(struct dp_power *power,
		struct sde_power_handle *phandle,
		struct drm_device *drm_dev);
	void (*power_client_deinit)(struct dp_power *power);
};

/**
 * dp_power_get() - configure and get the DisplayPort power module data
 *
 * @parser: instance of parser module
 * return: pointer to allocated power module data
 *
 * This API will configure the DisplayPort's power module and provides
 * methods to be called by the client to configure the power related
 * modueles.
 */
struct dp_power *dp_power_get(struct dp_parser *parser);

/**
 * dp_power_put() - release the power related resources
 *
 * @power: pointer to the power module's data
 */
void dp_power_put(struct dp_power *power);
#endif /* _DP_POWER_H_ */
