// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/pwm.h>
#include <video/mipi_display.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>

#include <dsi_ctrl_hw.h>
#include <dsi_parser.h>
#include <dsi_panel.h>

#include "../include/surfacedisplay/surface_panel.h"
#include "../include/surfacedisplay/surface_dsi_defs.h"

struct mutex surface_compound_panel_lock;
/*Primary comes enabled from uefi, and we enable secondary in kernel*/
static uint8_t surface_active_tiles_ref_cnt = 1;
static bool freq_sel_gpio_taken = false;

int surface_panel_gpio_request(struct surface_panel *surface_priv_panel)
{
	int rc = 0;

	if (!surface_priv_panel) {
		DSI_ERR("invalid params, panel is NULL");
		return -EINVAL;
	}

	if (freq_sel_gpio_taken) {
		DSI_INFO("Gpio for freq_sel already taken, skip request\n", rc);
		return rc;
	}

	if (gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_sel_gpio)) {
		rc = gpio_request(surface_priv_panel->gpio_config.disp_freq_sel_gpio, "freq_sel_gpio");
		if (rc) {
			DSI_ERR("request for freq_sel_gpio failed, rc=%d\n", rc);
			goto error;
		}
		freq_sel_gpio_taken = true;
	} else {
		DSI_ERR("gpio freq_sel_gpio is invalid \n");
	}

	if (gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_force_gpio)) {
		rc = gpio_request(surface_priv_panel->gpio_config.disp_freq_force_gpio, "freq_force_gpio");
		if (rc) {
			DSI_ERR("request for freq_force_gpio failed, rc=%d\n", rc);
			goto error_release_freq_sel;
		}
	} else {
		DSI_ERR("gpio freq_force_gpio is invalid \n");
	}

	goto error;

error_release_freq_sel:
	if (gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_sel_gpio)) {
		gpio_free(surface_priv_panel->gpio_config.disp_freq_sel_gpio);
		freq_sel_gpio_taken = false;
	}
error:
	return rc;
}
EXPORT_SYMBOL(surface_panel_gpio_request);

int surface_panel_gpio_release(struct surface_panel *surface_priv_panel)
{
	int rc = 0;

	if (!surface_priv_panel) {
		DSI_ERR("invalid params, panel is NULL");
		return -EINVAL;
	}

	if (gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_force_gpio))
		gpio_free(surface_priv_panel->gpio_config.disp_freq_force_gpio);

	mutex_lock(&surface_compound_panel_lock);
	if (gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_sel_gpio)
			&& freq_sel_gpio_taken && surface_active_tiles_ref_cnt == 0) {
		gpio_free(surface_priv_panel->gpio_config.disp_freq_sel_gpio);
		freq_sel_gpio_taken = false;
	}
	mutex_unlock(&surface_compound_panel_lock);
	return rc;
}
EXPORT_SYMBOL(surface_panel_gpio_release);

static int surface_dsi_panel_tx_cmd_set(struct dsi_panel *panel,
				enum surface_dsi_cmd_set_type type)
{
	int rc = 0, i = 0;
	ssize_t len;
	struct dsi_cmd_desc *cmds;
	u32 count;
	enum dsi_cmd_set_state state;
	struct dsi_display_mode *mode;
	const struct mipi_dsi_host_ops *ops = panel->host->ops;

	if (!panel || !panel->cur_mode)
		return -EINVAL;

	mode = panel->cur_mode;

	cmds = mode->priv_info->cmd_sets[type].cmds;
	count = mode->priv_info->cmd_sets[type].count;
	state = mode->priv_info->cmd_sets[type].state;

	if (count == 0) {
		DSI_DEBUG("[%s] No commands to be sent for state(%d)\n",
			 panel->name, type);
		goto error;
	}

	for (i = 0; i < count; i++) {
		if (state == DSI_CMD_SET_STATE_LP)
			cmds->msg.flags |= MIPI_DSI_MSG_USE_LPM;

		if (cmds->last_command)
			cmds->msg.flags |= MIPI_DSI_MSG_LASTCOMMAND;

		if (type == DSI_CMD_SET_VID_TO_CMD_SWITCH)
			cmds->msg.flags |= MIPI_DSI_MSG_ASYNC_OVERRIDE;

		len = ops->transfer(panel->host, &cmds->msg);
		if (len < 0) {
			rc = len;
			DSI_ERR("failed to set cmds(%d), rc=%d\n", type, rc);
			goto error;
		}
		if (cmds->post_wait_ms)
			usleep_range(cmds->post_wait_ms*1000,
					((cmds->post_wait_ms*1000)+10));
		cmds++;
	}
error:
	return rc;
}

static int surface_dsi_pwr_panel_regulator_mode_set(struct dsi_regulator_info *regs,
						const char *reg_name,
						int regulator_mode)
{
	int i = 0, rc = 0;
	struct dsi_vreg *vreg;

	if (regs->count == 0)
		return -EINVAL;

	if (!regs->vregs)
		return -EINVAL;

	for (i = 0; i < regs->count; i++) {
		vreg = &regs->vregs[i];
		if (!strcmp(vreg->vreg_name, reg_name)) {
			rc = regulator_set_mode(vreg->vreg,
							regulator_mode);
			if (rc)
				DSI_ERR("Regulator %s set mode %d failed\n",
					vreg->vreg_name, rc);
			break;
		}
	}

	if (i >= regs->count) {
		DSI_ERR("Regulator %s was not found\n", reg_name);
		return -EINVAL;
	}

	return rc;
}

int surface_panel_parse_features(struct surface_panel *surface_priv_panel,
                        struct dsi_parser_utils *utils)
{
	int rc = 0;
	struct surface_elvss_config *elvss;

	if (!surface_priv_panel || !utils) {
		DSI_ERR("invalid params\n");
		return -EINVAL;
	}

	elvss = &surface_priv_panel->elvss;

	surface_priv_panel->ext_trigger_enable = utils->read_bool(utils->data,
                                            "surface-ext-trig-enable");
	if (!surface_priv_panel->ext_trigger_enable) {
		DSI_ERR("surface-ext-trig-enable disabled by default \n");
	}

	rc = utils->read_u32(utils->data, "surface-dsi-elvss-levels",
				&elvss->num_levels);

	if (rc) {
		DSI_ERR("failed to read surface-dsi-elvss-levels, rc=%d\n",
		       rc);
		elvss->num_levels = 0;
		rc = 0;
		goto error;
	}

	if (elvss->num_levels > SURFACE_DSI_ELVSS_LEVELS_MAX) {
		DSI_ERR("unsupported num of levels for ELVSS, using max %d", SURFACE_DSI_ELVSS_LEVELS_MAX);
		elvss->num_levels = SURFACE_DSI_ELVSS_LEVELS_MAX;
	}

	DSI_INFO("surface-ext-trig-enable %d \n", surface_priv_panel->ext_trigger_enable);
	DSI_INFO("surface-dsi-elvss-levels %d \n", elvss->num_levels);

error:
    return rc;
}
EXPORT_SYMBOL(surface_panel_parse_features);

int surface_panel_parse_gpios(struct surface_panel *surface_priv_panel,
							struct dsi_parser_utils *utils)
{
	int rc = 0;

	if (!surface_priv_panel || !utils) {
		DSI_ERR("invalid params\n");
		return -EINVAL;
	}

	surface_priv_panel->gpio_config.disp_freq_force_gpio = utils->get_named_gpio(utils->data,
					      "surface-platform-freq-force-gpio", 0);
	if (!gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_force_gpio)) {
		rc = surface_priv_panel->gpio_config.disp_freq_force_gpio;
		DSI_ERR("failed get disp freq force gpio, rc=%d\n", rc);
		goto error;
	}

	surface_priv_panel->gpio_config.disp_freq_sel_gpio = utils->get_named_gpio(utils->data,
					      "surface-platform-freq-sel-gpio", 0);
	if (!gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_sel_gpio)) {
		rc = surface_priv_panel->gpio_config.disp_freq_sel_gpio;
		DSI_ERR("failed get disp freq sel gpio, rc=%d\n", rc);
		goto error;
	}

	DSI_INFO("surface-platform-freq-force-gpio %d \n", surface_priv_panel->gpio_config.disp_freq_force_gpio);
	DSI_INFO("surface-platform-freq-sel-gpio %d \n", surface_priv_panel->gpio_config.disp_freq_sel_gpio);
error:
	return rc;
}
EXPORT_SYMBOL(surface_panel_parse_gpios);

int surface_panel_set_aod(void* panel_ptr, bool enable_aod)
{

	int rc = 0;

	struct dsi_panel* panel = (struct dsi_panel*)panel_ptr;

	if (!panel) {
		DSI_ERR("dsi_panel is NULL\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);
	if (!panel->panel_initialized)
		goto exit;

	if (enable_aod) {
		/*
		* Consider LP1->LP2->LP1.
		* If the panel is already in LP mode, do not need to
		* set the regulator.
		* IBB and AB power mode would be set at the same time
		* in PMIC driver, so we only call ibb setting that is enough.
		*/
		if (dsi_panel_is_type_oled(panel) &&
			panel->power_mode != SDE_MODE_DPMS_LP2)
			surface_dsi_pwr_panel_regulator_mode_set(&panel->power_info,
				"ibb", REGULATOR_MODE_IDLE);
		rc = surface_dsi_panel_tx_cmd_set(panel, SURFACE_DSI_CMD_AOD_ON);
		if (rc)
			DSI_ERR("[%s] failed to send SURFACE_DSI_CMD_AOD_ON cmd, rc=%d\n",
				panel->name, rc);
	} else {
		/*
		* Consider about LP1->LP2->NOLP.
		*/
		if (dsi_panel_is_type_oled(panel) &&
			(panel->power_mode == SDE_MODE_DPMS_LP1 ||
			panel->power_mode == SDE_MODE_DPMS_LP2))
			surface_dsi_pwr_panel_regulator_mode_set(&panel->power_info,
				"ibb", REGULATOR_MODE_NORMAL);
		rc = surface_dsi_panel_tx_cmd_set(panel, SURFACE_DSI_CMD_AOD_OFF);
		if (rc)
			DSI_ERR("[%s] failed to send SURFACE_DSI_CMD_AOD_OFF cmd, rc=%d\n",
				panel->name, rc);
	}
exit:
	mutex_unlock(&panel->panel_lock);
	return rc;
}
EXPORT_SYMBOL(surface_panel_set_aod);

int surface_panel_refresh_rate_switch_gpio(void* panel_ptr, u32 refresh_rate) {
	int rc = 0;
	struct dsi_panel *panel = (struct dsi_panel*)panel_ptr;
	struct surface_panel *surface_priv_panel =  NULL;
	uint8_t refresh_rate_gpio_60 = 0;
	uint8_t refresh_rate_gpio_90 = 1;

	if (!panel) {
		DSI_ERR("invalid params pane is NULL\n");
		return -EINVAL;
	}

	if (refresh_rate != 60 && refresh_rate != 90) {
		DSI_ERR("invalid refresh rate %d\n", refresh_rate);
		return -EINVAL;
	}

	surface_priv_panel = &panel->surface_priv_panel;

	mutex_lock(&surface_compound_panel_lock);
	if (gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_sel_gpio)) {
		rc = gpio_direction_output(surface_priv_panel->gpio_config.disp_freq_sel_gpio,
						refresh_rate == 60 ? refresh_rate_gpio_60 : refresh_rate_gpio_90);
		if (rc) {
			DSI_ERR(" could not flip GPIO for %d, rc = %d out-of-sync", refresh_rate, rc);
			rc = 0;
		}
		gpio_set_value(surface_priv_panel->gpio_config.disp_freq_sel_gpio,
				refresh_rate == 60 ? refresh_rate_gpio_60 : refresh_rate_gpio_90);
	}
	mutex_unlock(&surface_compound_panel_lock);
	return rc;
}
EXPORT_SYMBOL(surface_panel_refresh_rate_switch_gpio);

int surface_panel_control_freq_sel_gpio(void* panel_ptr, u32 refresh_rate, bool on) {
	int rc = 0;
	struct dsi_panel *panel = (struct dsi_panel*)panel_ptr;
	struct surface_panel *surface_priv_panel = NULL;

	if (!panel) {
		DSI_ERR("invalid panel handle\n");
		return -EINVAL;
	}
	surface_priv_panel = &panel->surface_priv_panel;

	if (!gpio_is_valid(surface_priv_panel->gpio_config.disp_freq_sel_gpio)) {
		DSI_ERR("invalid gpio request\n");
		return -EINVAL;
	}

	if (refresh_rate != 60 && refresh_rate != 90) {
		DSI_ERR("invalid refresh rate %d\n", refresh_rate);
		return -EINVAL;
	}

	mutex_lock(&surface_compound_panel_lock);

	if (!on) {
		surface_active_tiles_ref_cnt--;
		if (surface_active_tiles_ref_cnt == 0) {
			rc = gpio_direction_output(surface_priv_panel->gpio_config.disp_freq_sel_gpio, 0);
			if (rc) {
				DSI_ERR(" could not set freq_sel gpio output direction %d", rc);
				rc = 0;/*Dont fail as we will just wont have sync*/
			}
			gpio_set_value(surface_priv_panel->gpio_config.disp_freq_sel_gpio, 0);
		}
	} else {
		/* In this case we'll just update the ref counter
		 * and let setActiveConfig to configure both DSC and GPIO
		 * like any other mode switch on posture change
		 */
		surface_active_tiles_ref_cnt++;
	}

	mutex_unlock(&surface_compound_panel_lock);
	return rc;
}
EXPORT_SYMBOL(surface_panel_control_freq_sel_gpio);

int surface_panel_refresh_rate_switch_dcs(void*panel_ptr,
		enum surface_dsi_cmd_set_type type)
{
	int rc = 0;
	struct dsi_panel *panel = (struct dsi_panel*)panel_ptr;

	if (!panel) {
		DSI_ERR("invalid params pane is NULL\n");
		return -EINVAL;
	}

	if (type < SURFACE_DSI_CMD_SWITCH_RR_60 || type > SURFACE_DSI_CMD_SWITCH_RR_90) {
		DSI_ERR("invalid refresh rate\n");
		return -EINVAL;
	}

	mutex_lock(&panel->panel_lock);

	rc = surface_dsi_panel_tx_cmd_set(panel, type);
	if (rc)
		DSI_ERR("[%s] failed to send refresh rate cmds for %s rc=%d\n",
		       panel->name,
			   type == SURFACE_DSI_CMD_SWITCH_RR_60 ? "SURFACE_DSI_CMDS_SWITCH_RR_60" : "SURFACE_DSI_CMDS_SWITCH_RR_90",
			   rc);

	mutex_unlock(&panel->panel_lock);
	return rc;
}
EXPORT_SYMBOL(surface_panel_refresh_rate_switch_dcs);

void surface_panel_init(void) {
	mutex_init(&surface_compound_panel_lock);
}
EXPORT_SYMBOL(surface_panel_init);

void surface_panel_deinit(void) {
	DSI_INFO("Surface Panel destroyed\n");
}
EXPORT_SYMBOL(surface_panel_deinit);
MODULE_LICENSE("GPL");
