/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _DP_DISPLAY_H_
#define _DP_DISPLAY_H_

#include <linux/list.h>
#include <drm/drmP.h>
#include <drm/msm_drm.h>
#include <drm/sde_drm.h>
#include "dp_panel.h"

#define MAX_DP_ACTIVE_DISPLAY	3

enum dp_drv_state {
	PM_DEFAULT,
	PM_SUSPEND,
};

struct dp_display_info {
	u32 cell_idx;
	u32 intf_idx[DP_STREAM_MAX];
	u32 phy_idx;
};

struct dp_display_bond_displays {
	void *dp_display[MAX_DP_BOND_NUM];
	u32 dp_display_num;
};

struct dp_mst_drm_cbs {
	void (*hpd)(void *display, bool hpd_status);
	void (*hpd_irq)(void *display);
	void (*set_drv_state)(void *dp_display,
			enum dp_drv_state mst_state);
};

struct dp_mst_drm_install_info {
	void *dp_mst_prv_info;
	const struct dp_mst_drm_cbs *cbs;
};

struct dp_mst_caps {
	bool has_mst;
	u32 max_streams_supported;
	u32 max_dpcd_transaction_bytes;
	struct drm_dp_aux *drm_aux;
};

struct dp_display {
	struct drm_device *drm_dev;
	struct dp_bridge *bridge;
	struct drm_connector *base_connector;
	void *base_dp_panel;
	bool is_sst_connected;
	bool is_mst_supported;
	u32 max_pclk_khz;
	u32 max_hdisplay;
	u32 max_vdisplay;
	u32 no_mst_encoder;
	void *dp_mst_prv_info;
	void *dp_bond_prv_info;
	bool force_bond_mode;

	int (*enable)(struct dp_display *dp_display, void *panel);
	int (*post_enable)(struct dp_display *dp_display, void *panel);

	int (*pre_disable)(struct dp_display *dp_display, void *panel);
	int (*disable)(struct dp_display *dp_display, void *panel);

	int (*set_mode)(struct dp_display *dp_display, void *panel,
			struct dp_display_mode *mode);
	enum drm_mode_status (*validate_mode)(struct dp_display *dp_display,
			void *panel, struct drm_display_mode *mode);
	int (*get_modes)(struct dp_display *dp_display, void *panel,
		struct dp_display_mode *dp_mode);
	int (*prepare)(struct dp_display *dp_display, void *panel);
	int (*unprepare)(struct dp_display *dp_display, void *panel);
	int (*request_irq)(struct dp_display *dp_display);
	struct dp_debug *(*get_debug)(struct dp_display *dp_display);
	void (*post_open)(struct dp_display *dp_display);
	int (*config_hdr)(struct dp_display *dp_display, void *panel,
				struct drm_msm_ext_hdr_metadata *hdr_meta);
	int (*post_init)(struct dp_display *dp_display);
	int (*mst_install)(struct dp_display *dp_display,
			struct dp_mst_drm_install_info *mst_install_info);
	int (*mst_uninstall)(struct dp_display *dp_display);
	int (*mst_connector_install)(struct dp_display *dp_display,
			struct drm_connector *connector);
	int (*mst_connector_uninstall)(struct dp_display *dp_display,
			struct drm_connector *connector);
	int (*mst_connector_update_edid)(struct dp_display *dp_display,
			struct drm_connector *connector,
			struct edid *edid);
	int (*mst_connector_update_link_info)(struct dp_display *dp_display,
			struct drm_connector *connector);
	int (*mst_get_fixed_topology_port)(struct dp_display *dp_display,
			u32 strm_id, u32 *port_num);
	int (*get_mst_caps)(struct dp_display *dp_display,
			struct dp_mst_caps *mst_caps);
	int (*set_stream_info)(struct dp_display *dp_display, void *panel,
			u32 strm_id, u32 start_slot, u32 num_slots, u32 pbn,
			int vcpi);
	void (*convert_to_dp_mode)(struct dp_display *dp_display, void *panel,
			const struct drm_display_mode *drm_mode,
			struct dp_display_mode *dp_mode);
	int (*update_pps)(struct dp_display *dp_display,
			struct drm_connector *connector, char *pps_cmd);
	void (*wakeup_phy_layer)(struct dp_display *dp_display,
			bool wakeup);
	int (*get_display_type)(struct dp_display *dp_display,
			const char **display_type);
	int (*mst_get_fixed_topology_display_type)(
			struct dp_display *dp_display, u32 strm_id,
			const char **display_type);
	int (*set_phy_bond_mode)(struct dp_display *dp_display,
			enum dp_phy_bond_mode mode,
			struct drm_connector *primary_connector);
};

#if IS_ENABLED(CONFIG_DRM_MSM_DP)
int dp_display_get_num_of_boot_displays(void);
int dp_display_get_num_of_displays(void);
int dp_display_get_displays(void **displays, int count);
int dp_display_get_num_of_streams(void *dp_display);
int dp_display_get_num_of_bonds(void *dp_display);
int dp_display_get_info(void *dp_display, struct dp_display_info *dp_info);
int dp_display_get_bond_displays(void *dp_display, enum dp_bond_type type,
		struct dp_display_bond_displays *dp_bond_info);
int dp_display_cont_splash_config(void *display);
int dp_display_splash_res_cleanup(struct dp_display *dp_display);
#else
static inline int dp_display_get_num_of_boot_displays(void)
{
	return 0;
}
static inline int dp_display_get_num_of_displays(void)
{
	return 0;
}
static inline int dp_display_get_displays(void **displays, int count)
{
	return 0;
}
static inline int dp_display_get_num_of_streams(void *dp_display)
{
	return 0;
}
static inline int dp_display_get_num_of_bonds(void *dp_display)
{
	return 0;
}
static inline int dp_display_get_info(void *dp_display, struct dp_display_info *dp_info)
{
	return 0;
}
static inline int dp_display_get_bond_displays(void *dp_display, enum dp_bond_type type,
	struct dp_display_bond_displays *dp_bond_info)
{
	return 0;
}
static inline int dp_display_cont_splash_config(void *display)
{
	return 0;
}
static inline int dp_display_splash_res_cleanup(struct dp_display *dp_display)
{
	return 0;
}
#endif /* CONFIG_DRM_MSM_DP */
#endif /* _DP_DISPLAY_H_ */
