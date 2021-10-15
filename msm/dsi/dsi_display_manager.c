// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>

#include "msm_drv.h"
#include "sde_connector.h"
#include "sde_dbg.h"
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
#include "dsi_display_manager.h"

#define to_dsi_display(x) container_of(x, struct dsi_display, list)

static struct dsi_display *display_manager_get_master(void)
{
	struct list_head *pos, *tmp;
	struct dsi_display *display = NULL;

	list_for_each_safe(pos, tmp, &disp_mgr.display_list) {
		display = list_entry(pos,
				struct dsi_display, list);
		if (display->is_master)
			return display;
	}

	return display;
}

// to-do need to make this one and above a common api
static struct dsi_display *display_manager_get_slave(void)
{
	struct list_head *pos, *tmp;
	struct dsi_display *display = NULL;

	list_for_each_safe(pos, tmp, &disp_mgr.display_list) {
		display = list_entry(pos,
				struct dsi_display, list);
		if (!display->is_master)
			return display;
	}

	return display;
}

static void dsi_display_manager_init(void)
{
	if (!disp_mgr.init) {
		INIT_LIST_HEAD(&disp_mgr.display_list);
		mutex_init(&disp_mgr.disp_mgr_mutex);
		disp_mgr.init = true;
	}
}

static void dsi_display_manager_view(void)
{
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, &disp_mgr.display_list) {
		struct dsi_display *display = list_entry(pos,
				struct dsi_display, list);
		DSI_INFO("display name is %s\n", display->panel->name);
		DSI_INFO("display type is %s\n", display->panel->type);
	}
}

void dsi_display_manager_register(struct dsi_display *display)
{
	struct dsi_ctrl *ctrl;
	struct dsi_display_ctrl *display_ctrl;

	if (display->ctrl_count > 1) {
		DSI_DEBUG("split display case, no need of a mgr\n");
		return;
	}

	/* for dual display each display will use one ctrl */
	display_ctrl = &display->ctrl[0];
	ctrl = display_ctrl->ctrl;

	dsi_display_manager_init();
	list_add(&display->list, &disp_mgr.display_list);

	DSI_DEBUG("cell_index = %d\n", ctrl->cell_index);

	/* mark the ctrl0 as master */
	if (display->panel->ctl_op_sync) {
		if (ctrl->cell_index == 0)
			display->is_master = true;
	}

	dsi_display_manager_view();
}

void dsi_display_manager_unregister(struct dsi_display *display)
{
	struct dsi_ctrl *ctrl;
	struct dsi_display_ctrl *display_ctrl;

	if (display->ctrl_count > 1) {
		DSI_DEBUG("split display case, no need of a mgr\n");
		return;
	}

	display_ctrl = &display->ctrl[0];
	ctrl = display_ctrl->ctrl;

	DSI_INFO("%s cell_index = %d\n", __func__,
			ctrl->cell_index);

	list_del(&display->list);
}

static dsi_display_mgr_phy_control_enable(struct dsi_display *display,
		enum dsi_display_mgr_ctrl_type type)
{
	struct msm_dsi_phy *phy;
	struct msm_dsi_phy *m_phy;
	struct dsi_display *m_display;
	int ret = 0;

	/* each display will have one controller in dual display case */
	mutex_lock(&disp_mgr.disp_mgr_mutex);
	phy = display->ctrl[0].phy;

	/*
	 * incoming display might be master or slave so get a handle
	 * to master
	 */
	m_display = display_manager_get_master();
	m_phy = m_display->ctrl[0].phy;

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY, type);

	/*
	 * Check the refcount, if its the first enable, then
	 * Check if its the master display, if it is, enable it first
	 * else, get the master display from the list and enable it first
	 */

	if (phy->sync_en_refcount > 0)
		goto not_first_enable;

	if (display->is_master) {
		if (type == DSI_DISPLAY_MGR_PHY_PWR) {
			ret = dsi_display_phy_sw_reset(display);
			if (ret) {
				DSI_ERR("failed to reset master%d\n", ret);
				goto error;
			}
			ret = dsi_display_phy_enable(display, DSI_PLL_SOURCE_NATIVE);
			if (ret) {
				DSI_ERR("failed to enable master%d\n", ret);
				goto error;
			}
		} else if (type == DSI_DISPLAY_MGR_PHY_IDLE) {
			ret = dsi_display_phy_idle_on(display, display->clamp_enabled,
					DSI_PLL_SOURCE_NATIVE);
			if (ret) {
				DSI_ERR("failed to phy_idle_on master%d\n", ret);
				goto error;
			}
		}
	} else {
		/*
		 * if the master has not yet been enabled, enable it
		 * first. We need to enable the DSI CORE_CLK here to
		 * satisfy the requirement of phy_sw_reset that controller
		 * power needs to be enabled before the reset.
		 */
		if (m_display && (m_phy->sync_en_refcount == 0)) {
			if (m_phy->dsi_phy_state == DSI_PHY_ENGINE_OFF) {
				/*
				 * idle cases are triggered from the display_clk_ctrl context
				 * calling dsi_display_clk_ctrl() again will result in deadlock.
				 * Hence use the no-lock version of the API.
				 */
				ret = dsi_display_clk_ctrl_nolock(m_display->dsi_clk_handle,
						DSI_CORE_CLK, DSI_CLK_ON);
				if (ret) {
					DSI_ERR("failed to enable core clk on master%d\n", ret);
					goto error;
				}

				ret = dsi_display_phy_sw_reset(m_display);
				if (ret) {
					DSI_ERR("failed to reset master%d\n", ret);
					dsi_display_clk_ctrl_nolock(m_display->dsi_clk_handle,
							DSI_CORE_CLK, DSI_CLK_OFF);
					goto error;
				}

				/*
				 * PHY timings are updated usually as part of display_set_mode.
				 * In a use case when the slave PHY is turning on before the
				 * master, display_set_mode wouldn't have been called for the
				 * slave display. Therefore it is required to explicitly
				 * call the update_phy_timings op on master controller before
				 * enabling the master PHY.
				 *
				 * NOTE: These updated PHY timings may be slightly different from
				 * the devicetree as these are calculated within the driver, but
				 * as the display is not yet on it shouldn't cause any issues.
				 */
				DSI_DEBUG("Updating PHY timings before turning on PHY\n");
				dsi_phy_update_phy_timings(m_phy, &display->config);

				ret = dsi_display_phy_enable(m_display, DSI_PLL_SOURCE_NATIVE);
				if (ret) {
					DSI_ERR("failed to enable master%d\n", ret);
					dsi_display_clk_ctrl_nolock(m_display->dsi_clk_handle,
							DSI_CORE_CLK, DSI_CLK_OFF);
					goto error;
				}
				ret = dsi_display_clk_ctrl_nolock(m_display->dsi_clk_handle,
						DSI_CORE_CLK, DSI_CLK_OFF);
				if (ret) {
					DSI_ERR("failed to disable core clk on master%d\n", ret);
					goto error;
				}
			} else {
				/*
				 * when master refcnt is 0 but phy is still on,
				 * its idle case here we do not need to reset the
				 * phy on master so no clk vote needed in this case
				 */
				ret = dsi_display_phy_idle_on(m_display,
						display->clamp_enabled, DSI_PLL_SOURCE_NATIVE);
				if (ret) {
					DSI_ERR("failed to idle on master%d\n", ret);
					goto error;
				}
			}
		}

		if (!ret) {
			if (type == DSI_DISPLAY_MGR_PHY_PWR) {
				ret = dsi_display_phy_sw_reset(display);
				if (ret) {
					DSI_ERR("failed to reset slave %d\n", ret);
					goto error;
				}
				ret = dsi_display_phy_enable(display, DSI_PLL_SOURCE_NON_NATIVE);
				if (ret) {
					DSI_ERR("failed to enable slave %d\n", ret);
					goto error;
				}
			} else if (type == DSI_DISPLAY_MGR_PHY_IDLE) {
				ret = dsi_display_phy_idle_on(display,
						display->clamp_enabled,
						DSI_PLL_SOURCE_NON_NATIVE);
				if (ret) {
					DSI_ERR("error phy_idle_on slave phy%d\n", ret);
					goto error;
				}
			}
			/*
			 * need to reprogram slave when powering up and coming
			 * out of idle
			 */
			ret = dsi_pll_program_slave(phy->pll);
			if (ret) {
				DSI_ERR("failed to program slave %d\n", ret);
				goto error;
			}
		}
	}

not_first_enable:
	/* add refcnt on the master phy if it was slave*/
	if (!display->is_master)
		m_phy->sync_en_refcount++;

	/* add refcnt on itself */
	phy->sync_en_refcount++;

error:
	DSI_DEBUG("master: %d phy ref_cnt = %d m_phy ref_cnt = %d\n",
			display->is_master, phy->sync_en_refcount,
			m_phy->sync_en_refcount);

	SDE_EVT32(display->is_master, phy->sync_en_refcount,
			m_phy->sync_en_refcount);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT, type);

	mutex_unlock(&disp_mgr.disp_mgr_mutex);

	return ret;
}

static dsi_display_mgr_phy_control_disable(struct dsi_display *display,
		enum dsi_display_mgr_ctrl_type type)
{
	struct msm_dsi_phy *m_phy;
	struct msm_dsi_phy *phy;
	struct dsi_display *m_display;
	struct dsi_display *s_display;
	struct msm_dsi_phy *s_phy;

	int ret = 0;

	mutex_lock(&disp_mgr.disp_mgr_mutex);

	phy = display->ctrl[0].phy;
	m_display = display_manager_get_master();
	m_phy = m_display->ctrl[0].phy;

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY, type);

	/* display decrements its own refcnt */
	phy->sync_en_refcount--;
	/* if its slave then it decrements master as well */
	if (!display->is_master)
		m_phy->sync_en_refcount--;

	/*
	 * If the refcount is 0 and its not master, disable phy
	 * If its not master, then also check if its safe to disable
	 * master since it was left enabled during its disable
	 * If its master, then disable only if slave was already disabled
	 * else wait for the disable of the slave to turn off master
	 */
	if (phy->sync_en_refcount > 0)
		goto not_last_disable;

	if (!display->is_master) {
		if (type == DSI_DISPLAY_MGR_PHY_PWR) {
			ret = dsi_display_phy_disable(display);
			if (ret)
				DSI_ERR("failed to disable slave%d\n", ret);
			/* disable master phy if it wasnt being used */
			if (m_phy->sync_en_refcount == 0) {
				ret = dsi_display_phy_disable(m_display);
				if (ret)
					DSI_ERR("failed to disable master%d\n", ret);
			}
		} else if (type == DSI_DISPLAY_MGR_PHY_IDLE) {
			ret = dsi_display_phy_idle_off(display);
			if (ret)
				DSI_ERR("failed to disable slave%d\n", ret);

			if (m_phy->sync_en_refcount == 0) {
				ret = dsi_display_phy_idle_off(m_display);
				if (ret)
					DSI_ERR("failed to phy_idle_off master%d\n", ret);
			}
		}
	} else {
		s_display = display_manager_get_slave();
		s_phy = s_display->ctrl[0].phy;
		if (s_phy->sync_en_refcount == 0) {
			if (type == DSI_DISPLAY_MGR_PHY_PWR) {
				ret = dsi_display_phy_disable(display);
				if (ret)
					DSI_ERR("failed to disable master%d\n", ret);
			} else if (type == DSI_DISPLAY_MGR_PHY_IDLE) {
				ret = dsi_display_phy_idle_off(display);
				if (ret)
					DSI_ERR("failed to disable master%d\n", ret);
			}
		}
	}

not_last_disable:
	DSI_DEBUG("master: %d phy ref_cnt = %d m_phy ref_cnt = %d\n",
			display->is_master, phy->sync_en_refcount,
			m_phy->sync_en_refcount);

	SDE_EVT32(display->is_master, phy->sync_en_refcount,
			m_phy->sync_en_refcount);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT, type);

	mutex_unlock(&disp_mgr.disp_mgr_mutex);

	return ret;
}


int dsi_display_mgr_phy_enable(struct dsi_display *display)
{
	int ret = 0;

	if (!display->panel->ctl_op_sync) {
		ret = dsi_display_phy_sw_reset(display);
		if (!ret)
			return dsi_display_phy_enable(display,
					DSI_PLL_SOURCE_STANDALONE);
	}

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

	ret = dsi_display_mgr_phy_control_enable(display,
			DSI_DISPLAY_MGR_PHY_PWR);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);

	return ret;
}

int dsi_display_mgr_phy_disable(struct dsi_display *display)
{
	int ret = 0;

	if (!display->panel->ctl_op_sync)
		return dsi_display_phy_disable(display);

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

	ret = dsi_display_mgr_phy_control_disable(display,
			DSI_DISPLAY_MGR_PHY_PWR);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);

	return ret;
}

int dsi_display_mgr_phy_idle_off(struct dsi_display *display)
{
	int ret = 0;

	if (!display->panel->ctl_op_sync)
		return dsi_display_phy_idle_off(display);

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

	ret = dsi_display_mgr_phy_control_disable(display,
			DSI_DISPLAY_MGR_PHY_IDLE);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);

	return ret;
}

int dsi_display_mgr_phy_idle_on(struct dsi_display *display)
{
	int ret = 0;

	if (!display->panel->ctl_op_sync) {
		return dsi_display_phy_idle_on(display,
				display->clamp_enabled, DSI_PLL_SOURCE_STANDALONE);
	}

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY);

	ret = dsi_display_mgr_phy_control_enable(display,
			DSI_DISPLAY_MGR_PHY_IDLE);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT);

	return ret;
}

int dsi_display_mgr_config_clk_gating(struct dsi_display *display, bool en)
{
	int ret = 0;
	struct dsi_display *m_display;

	if (!display->panel->ctl_op_sync)
		return dsi_display_config_clk_gating(display, en);

	SDE_EVT32(SDE_EVTLOG_FUNC_ENTRY, en);

	mutex_lock(&disp_mgr.disp_mgr_mutex);

	if (display->is_master) {
		ret = dsi_display_config_clk_gating(display, en);
		if (ret < 0)
			DSI_ERR("clk gating failed on master\n");
	} else {
		m_display = display_manager_get_master();
		if (!m_display) {
			DSI_ERR("master display not found\n");
			mutex_unlock(&disp_mgr.disp_mgr_mutex);
			return -EINVAL;
		}

		/* applying gate/ungate on slave */
		ret = dsi_display_config_clk_gating(display, en);
		if (ret < 0)
			DSI_ERR("clk gating failed on slave\n");

		/* applying gate/ungate on master */
		ret = dsi_display_config_clk_gating(m_display, en);
		if (ret < 0)
			DSI_ERR("clk gating failed on master\n");
	}

	mutex_unlock(&disp_mgr.disp_mgr_mutex);

	SDE_EVT32(SDE_EVTLOG_FUNC_EXIT, en);

	return ret;
}

int dsi_display_mgr_get_pll_info(struct dsi_display *display,
		enum dsi_pll_info info)
{
	struct msm_dsi_phy *phy;
	struct dsi_display *m_display;

	if (!display->panel->ctl_op_sync) {
		phy = display->ctrl[0].phy;
	} else {
		m_display = display_manager_get_master();
		phy = m_display->ctrl[0].phy;
	}

	return dsi_phy_get_pll_info(phy, info);
}
