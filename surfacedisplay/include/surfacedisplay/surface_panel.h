/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _SURFACE_PANEL_H_
#define _SURFACE_PANEL_H_

struct dsi_parser_utils;
enum surface_dsi_cmd_set_type;

/**
 * enum surface_panel_hint_type  - Surface panel hint type
 * @SURFACE_PANEL_PRIM      Primary
 * @SURFACE_PANEL_SEC       Secondary
 * @SURFACE_PANEL_FULL      Full
 */
enum surface_panel_hint_type {
	SURFACE_PANEL_PRIM = 0,
    SURFACE_PANEL_SEC,
	SURFACE_PANEL_FULL
};

/**
 * struct surface_elvss_config - Config for ELVSS
 * @num_levels:             Number of elvss levels
 * @elvss_level:            Current elvss level
 */
struct surface_elvss_config {
	u32 num_levels;
	u32 elvss_level;
};

/**
 * struct surface_panel_gpio_config - Config for GPIOs
 * @disp_freq_sel_gpio:     Freq select GPIO
 * @disp_freq_force_gpio:   Freq force GPIO
 */
struct surface_panel_gpio_config {
	int disp_freq_sel_gpio;
	int disp_freq_force_gpio;
};

/**
 * struct surface_panel - Surface panel
 * @ext_trigger_enable: Flag to enable ext trigger.
 * @refresh_rate:       refresh rate, taken from drm mode
 * @elvss_level:        Elvss level config
 */
struct surface_panel {
	bool ext_trigger_enable;
	bool aod_enabled;
	u32 refresh_rate;
	struct surface_elvss_config elvss;
	struct surface_panel_gpio_config gpio_config;
	enum surface_panel_hint_type hint;
	bool parent_dsi_panel_valid;
	void* parent_dsi_panel;
};

int surface_panel_parse_features(struct surface_panel *surface_priv_panel,
                        struct dsi_parser_utils *utils);

int surface_panel_parse_gpios(struct surface_panel *surface_priv_panel,
						struct dsi_parser_utils *utils);

int surface_panel_gpio_request(struct surface_panel *surface_priv_panel);

int surface_panel_gpio_release(struct surface_panel *surface_priv_panel);

int surface_panel_set_aod(void* panel_ptr, bool enable_aod);

int surface_panel_refresh_rate_switch_gpio(void* panel_ptr, u32 refresh_rate);

int surface_panel_refresh_rate_switch_dcs(void* panel_ptr,
		enum surface_dsi_cmd_set_type type);
int surface_panel_control_freq_sel_gpio(void* panel_ptr, u32 refresh_rate, bool ctrl);

void surface_panel_init(void);

void surface_panel_deinit(void);
#endif /* _SURFACE_PANEL_H_ */
