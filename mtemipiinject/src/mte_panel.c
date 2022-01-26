// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/file.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/delay.h>

#include "dsi_drm.h"

void* primary_panel = NULL;
void* second_panel = NULL;
struct dsi_display *mte_display_p = NULL;
struct dsi_display *mte_display_s = NULL;
unsigned int te_irq_p;
unsigned int te_irq_s;

/* mte_panel_get_time - restore r2 or c3's refresh rate */
void mte_panel_get_time(void* data) {
	struct dsi_display *display = (struct dsi_display *)data;
	static int n_frame = 0;
	static int c3_n_frame = 0;
	static int r2_n_frame = 0;
	int panel_id = display->panel->mte_priv_panel.panel_id;
	unsigned long timestamp_ms;

	if(panel_id == 2){
		if(c3_n_frame == display->panel->mte_priv_panel.rr_get_nframe
			&& r2_n_frame == display->panel->mte_priv_panel.rr_get_nframe){
			struct dsi_panel *p_panel;
			struct dsi_panel *s_panel;
			struct mte_panel *mte_p_panel = NULL;
			struct mte_panel *mte_s_panel = NULL;
			p_panel = (struct dsi_panel*)primary_panel;
			s_panel = (struct dsi_panel*)second_panel;
			mte_p_panel = &p_panel->mte_priv_panel;
			mte_s_panel = &s_panel->mte_priv_panel;
			mte_p_panel->rr_get_enabled = false;
			mte_s_panel->rr_get_enabled = false;
			c3_n_frame = 0;
			r2_n_frame = 0;
		}
		else if(r2_n_frame < display->panel->mte_priv_panel.rr_get_nframe
			&&(!strcmp(display->panel->type, "secondary"))) {
			timestamp_ms = ktime_to_us(ktime_get());
			display->panel->mte_priv_panel.timestamp[r2_n_frame] = timestamp_ms;
			r2_n_frame++;
		}
		else if(c3_n_frame < display->panel->mte_priv_panel.rr_get_nframe
			&&(!strcmp(display->panel->type, "primary"))) {
			timestamp_ms = ktime_to_us(ktime_get());
			display->panel->mte_priv_panel.timestamp[c3_n_frame] = timestamp_ms;
			c3_n_frame++;
		}
	}
	else{
		if(panel_id != !strcmp(display->panel->type, "secondary")){
			DSI_ERR("panel id and display name mismatch\n");
			return;
		}

		if(n_frame < display->panel->mte_priv_panel.rr_get_nframe){
			timestamp_ms = ktime_to_us(ktime_get());
			display->panel->mte_priv_panel.timestamp[n_frame] = timestamp_ms;
			n_frame++;
		}

		else if(n_frame == display->panel->mte_priv_panel.rr_get_nframe) {
			display->panel->mte_priv_panel.rr_get_enabled = false;
			n_frame = 0;
		}
	}
}
EXPORT_SYMBOL(mte_panel_get_time);

/* mte_panel_get_refresh_rate save refresh_rate on the file */
int mte_panel_get_refresh_rate(int panel_id, int nframe) {
	int rc = 0;
	int i;
	struct dsi_panel *panel = NULL;
	struct dsi_panel *s_panel = NULL;
	struct mte_panel *mte_panel = NULL;
	struct mte_panel *mte_s_panel = NULL;
	struct file *fptr;
	char data[256];
	int block_size = 0;
	mm_segment_t old_fs;
	loff_t pos = 0;


	if (panel_id < 0 || panel_id > 2) {
		DSI_ERR("invalid params panel_id %d\n", panel_id);
		return -EINVAL;
	}

	if (panel_id == 0){
		panel = (struct dsi_panel*)primary_panel;
	}
	else if(panel_id == 1){
		panel = (struct dsi_panel*)second_panel;
	}
	else{
		panel = (struct dsi_panel*)primary_panel;
		s_panel = (struct dsi_panel*)second_panel;
	}

	if (!panel) {
		DSI_ERR("invalid params panel is NULL\n");
		return -EINVAL;
	}

	if(!mte_display_p || !mte_display_s){
		DSI_ERR("display is not initialized\n");
		return -EINVAL;
	}

	//enable irq
	enable_irq(te_irq_p);
	mte_display_p->is_te_irq_enabled = true;
	enable_irq(te_irq_s);
	mte_display_s->is_te_irq_enabled = true;

	//initialize mte_panel setting
	mte_panel = &panel->mte_priv_panel;
	mte_panel->timestamp = kmalloc(sizeof(unsigned long)*nframe, GFP_KERNEL);
	mte_panel->panel_id = panel_id;
	mte_panel->rr_get_nframe = nframe;
	mte_panel->rr_get_enabled = true;

	if(panel_id == 2){
		mte_s_panel = &s_panel->mte_priv_panel;
		mte_s_panel->timestamp = kmalloc(sizeof(unsigned long)*nframe, GFP_KERNEL);
		mte_s_panel->panel_id = panel_id;
		mte_s_panel->rr_get_nframe = nframe;
		mte_s_panel->rr_get_enabled = true;
	}

        //while (mte_priv_panel->rr_get_enabled);
	if(panel_id == 2){
		while(mte_panel->rr_get_enabled || mte_s_panel->rr_get_enabled){
			mdelay(1000);
		}
	}
	else{
		while(mte_panel->rr_get_enabled){
			mdelay(1000);
		}
	}

	//disable irq
	disable_irq(te_irq_p);
	mte_display_p->is_te_irq_enabled = false;
	disable_irq(te_irq_s);
	mte_display_s->is_te_irq_enabled = false;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	if(panel_id == 0){
		fptr = filp_open("/data/vendor/display/DISGET_RR_0.txt", O_WRONLY | O_CREAT, 0644);
	}
	else if(panel_id == 1){
		fptr = filp_open("/data/vendor/display/DISGET_RR_1.txt", O_WRONLY | O_CREAT, 0644);
	}
	else{
		fptr = filp_open("/data/vendor/display/DISPGET_TWOPANEL_RR.txt", O_WRONLY | O_CREAT, 0644);
	}

	if(IS_ERR(fptr)){
		DSI_ERR("File open failed, panel_id = %d\n", panel_id);
		return -EINVAL;
	}

	if(panel_id == 2){
		block_size = snprintf(data, 200, "Panel_ID0, FrameIndex(Panel_ID0), TimeStamp(panel_ID0)(us), Panel_ID1, FrameIndex(Panel_ID1), TimeStamp(Panel_ID1)(us)\n");
		vfs_write(fptr, data, block_size, &pos);

		for (i = 0; i < nframe; i++) {
			block_size = snprintf(data, 60, "0, %d, %lu, 1, %d, %lu\n",
				i, mte_panel->timestamp[i], i, mte_s_panel->timestamp[i]);
			vfs_write(fptr, data, block_size, &pos);
		}
	}
	else{
		block_size = snprintf(data, 60, "PanelID, FrameIndex, TimeStamp(us)\n");
		vfs_write(fptr, data, block_size, &pos);

		for (i = 0; i < nframe; i++) {
			block_size = snprintf(data, 30, "%d, %d, %lu\n",
				panel_id, i, mte_panel->timestamp[i]);
			vfs_write(fptr, data, block_size, &pos);
		}
	}
	filp_close(fptr, NULL);
	set_fs(old_fs);

	return rc;
}
EXPORT_SYMBOL(mte_panel_get_refresh_rate);

void mte_set_panel_ptr(void* panel, bool primary) {
	if(primary)
		primary_panel = panel;
	else
		second_panel = panel;
}
EXPORT_SYMBOL(mte_set_panel_ptr);

void* mte_get_panel_ptr(int panel_id) {
	if(panel_id == 0)
		if(primary_panel != NULL)
			return primary_panel;
	if(panel_id == 1)
		if(second_panel != NULL)
			return second_panel;
	return NULL;
}
EXPORT_SYMBOL(mte_get_panel_ptr);

/* sysfs_rr_write: write the timetable on get_rr sysfs */
static ssize_t sysfs_rr_write(struct device *dev,
	struct device_attribute *attr, char *buf){
	ssize_t ret = 0;
	int i;
	struct mte_panel *mte_p_panel = NULL;
	struct mte_panel *mte_s_panel = NULL;
	struct dsi_panel *p_panel = NULL;
	struct dsi_panel *s_panel = NULL;
	char temp[25];

	p_panel = (struct dsi_panel*)primary_panel;
	s_panel = (struct dsi_panel*)second_panel;
	mte_p_panel = &p_panel->mte_priv_panel;
	mte_s_panel = &s_panel->mte_priv_panel;

	//wait a sec, until both panel timetable is filled.
	while(mte_p_panel->rr_get_enabled || mte_s_panel->rr_get_enabled){
		mdelay(1000);
	}

	if(mte_p_panel->timestamp != NULL){
		for(i=0; i < mte_p_panel->rr_get_nframe; i++){
			snprintf(temp,sizeof(temp),"0,%d,%lu\n",i, mte_p_panel->timestamp[i]);
			strcat(buf, temp);
		}
		kfree(mte_p_panel->timestamp);
		mte_p_panel->timestamp = NULL;
	}

	if(mte_s_panel->timestamp != NULL){
		for(i=0; i < mte_s_panel->rr_get_nframe; i++){
			snprintf(temp,sizeof(temp),"1,%d,%lu\n",i, mte_s_panel->timestamp[i]);
			strcat(buf, temp);
		}
		kfree(mte_s_panel->timestamp);
		mte_s_panel->timestamp = NULL;
	}

	ret = strlen(buf);
	return ret;
}

/* sysfs_rr_parse: get_rr has been called, parse the input */
static ssize_t sysfs_rr_parse(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count) {
	unsigned long panel_id, nframe;
	char *buf_ptr, *tok;
	struct dsi_display *display = dev_get_drvdata(dev);

	if (buf == NULL) {
		DSI_ERR("[%s] sysfs_rr_parse buf == NULL\n:",
			display->name);
		return -1;
	}

	buf_ptr = (char *)buf;
	tok = strsep(&buf_ptr, ",");
	if (tok == NULL) {
		DSI_ERR("Missing panel id\n");
		return -EINVAL;
	}

	kstrtol(tok, 10, &panel_id);

	kstrtol(buf_ptr, 10, &nframe);

	mte_panel_get_refresh_rate((int) panel_id, (int) nframe);
	return count;
}

/* get_rr device attribute setting */
static DEVICE_ATTR(get_rr, 0644,
			sysfs_rr_write,
			sysfs_rr_parse);

static struct attribute *get_rr_fs_attrs[] = {
	&dev_attr_get_rr.attr,
	NULL,
};

static struct attribute_group get_rr_fs_attrs_group = {
	.attrs = get_rr_fs_attrs,
};

/* mte_display_inject_sysfs_init: initialize sysfs */
int mte_display_inject_sysfs_init(struct device *dev) {
	int rc = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct dsi_display *display = platform_get_drvdata(pdev);

	if(!mte_display_p) {
		mte_display_p = display;
		te_irq_p = gpio_to_irq(display->disp_te_gpio);
	}
	else {
		mte_display_s = display;
		te_irq_s = gpio_to_irq(display->disp_te_gpio);
	}

	if (display->panel->panel_mode == DSI_OP_CMD_MODE) {
		rc = sysfs_create_group(&dev->kobj,
				&get_rr_fs_attrs_group);
	}
	return rc;
}

/* mte_display_inject_sysfs_deinit: deinit sysfs */
void mte_display_inject_sysfs_deinit(struct device *dev) {
	struct platform_device *pdev = to_platform_device(dev);
	struct dsi_display *display = platform_get_drvdata(pdev);
	if (display->panel->panel_mode == DSI_OP_CMD_MODE) {
		sysfs_remove_group(&dev->kobj,
				&get_rr_fs_attrs_group);
	}
}
