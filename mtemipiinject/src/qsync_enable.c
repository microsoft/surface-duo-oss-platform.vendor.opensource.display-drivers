#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/string.h>

#include "dsi_drm.h"


int surface_panel_refresh_rate_switch_dcs(void *panel_ptr,
                                          enum surface_dsi_cmd_set_type type);
int surface_panel_refresh_rate_switch_gpio(void *panel_ptr, u32 refresh_rate);


/* Switch DCS */

static ssize_t sysfs_switch_dcs_write(struct device *dev,
                                  struct device_attribute *attr, const char *buf, size_t count) {

   struct dsi_display *dsi_display = dev_get_drvdata(dev);
   long freq;
   int rc;

   if (dsi_display == NULL || dsi_display->panel == NULL ||
           buf == NULL || count == 0) {
           pr_err("Wrong argument");
           return -EINVAL;
   }

   kstrtol(buf, 10, &freq);

   if (freq != 60 && freq != 90) {
           pr_err("Wrong argument");
           return -EINVAL;
   }

   rc = surface_panel_refresh_rate_switch_dcs(dsi_display->panel,
                                         (freq == 60 ? SURFACE_DSI_CMD_SWITCH_RR_60 :
                                                       SURFACE_DSI_CMD_SWITCH_RR_90));

   if (rc < 0) {
           pr_err("Failed switching dcs");
           return -EIO;
   }

   return strlen(buf);
}

static DEVICE_ATTR(switch_dcs, 0220,
			NULL,
			sysfs_switch_dcs_write);

static struct attribute *switch_dcs_fs_attrs[] = {
	&dev_attr_switch_dcs.attr,
	NULL,
};
static struct attribute_group switch_dcs_fs_attrs_group = {
	.attrs = switch_dcs_fs_attrs,
};


/* Switch GPIO */

static ssize_t sysfs_switch_gpio_write(struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t count) {
        struct dsi_display *dsi_display = dev_get_drvdata(dev);
        long freq;
        int rc;

        if (dsi_display == NULL || dsi_display->panel == NULL ||
                buf == NULL || count == 0) {
                pr_err("Wrong argument");
                return -EINVAL;
        }

        kstrtol(buf, 10, &freq);

        if (freq != 60 && freq != 90) {
                pr_err("Wrong argument");
                return -EINVAL;
        }

        rc = surface_panel_refresh_rate_switch_gpio(dsi_display->panel, (u32)freq);
        if (rc < 0) {
                pr_err("Failed switching gpio");
                return -EIO;
        }

        return strlen(buf);
}

static DEVICE_ATTR(switch_gpio, 0220,
			NULL,
			sysfs_switch_gpio_write);

static struct attribute *switch_gpio_fs_attrs[] = {
	&dev_attr_switch_gpio.attr,
	NULL,
};
static struct attribute_group switch_gpio_fs_attrs_group = {
	.attrs = switch_gpio_fs_attrs,
};



int qsync_enable_sysfs_init(struct dsi_display *display) {

        struct device *dev = &display->pdev->dev;
        int rc;

        rc = sysfs_create_group(&dev->kobj,
                           &switch_dcs_fs_attrs_group);
        if (rc < 0) {
                pr_err("Failed creating switch_dcs node");
                return -EIO;
        }

        rc = sysfs_create_group(&dev->kobj,
                           &switch_gpio_fs_attrs_group);
        if (rc < 0) {
                pr_err("Failed creating switch_dcs node");
                return -EIO;
        }

        return rc;
}

int qsunc_enable_sysfs_deinit(struct dsi_display *display) {

        struct device *dev = &display->pdev->dev;

        sysfs_remove_group(&dev->kobj,
                                   &switch_dcs_fs_attrs_group);
        sysfs_remove_group(&dev->kobj,
                                   &switch_gpio_fs_attrs_group);
        return 0;

}
