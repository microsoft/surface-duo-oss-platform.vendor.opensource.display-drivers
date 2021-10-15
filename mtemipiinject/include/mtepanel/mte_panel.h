/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _MTE_PANEL_H_
#define _MTE_PANEL_H_

struct mte_panel {
	bool rr_get_enabled;
	bool primary;
	int panel_id;
	int rr_get_nframe;
	unsigned long* timestamp;
	void* panel;
};

void mte_panel_get_time(void *data);

int mte_panel_get_refresh_rate(int panel_id, int nframe);

void mte_set_panel_ptr(void* panel, bool primary);

void* mte_get_panel_ptr(int panel_id);

int mte_display_inject_sysfs_init(struct device *dev);

void mte_display_inject_sysfs_deinit(struct device *dev);
#endif /* _MTE_PANLE_H_ */
