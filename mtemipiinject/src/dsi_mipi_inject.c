#include <linux/list.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/string.h>

#include "dsi_drm.h"

#define MIPI_RD_BUF_SZ    1024

ssize_t mipi_dsi_dcs_write(struct mipi_dsi_device *dsi, u8 cmd,
			   const void *data, size_t len);
ssize_t mipi_dsi_dcs_read(struct mipi_dsi_device *dsi, u8 cmd, void *data,
			  size_t len);

int qsync_enable_sysfs_init(struct dsi_display *display);
int qsunc_enable_sysfs_deinit(struct dsi_display *display);

static u8 mipi_rd_buf[MIPI_RD_BUF_SZ] = { 0 };
static size_t mipi_rd_buf_len = 0;

/* MIPI RD */
static ssize_t sysfs_mipird_read(struct device *dev,
	struct device_attribute *attr, char *buf) {
	ssize_t ret = 0;
	char temp[16];
	u16 i;

	if (mipi_rd_buf_len == 0) {
		return 0;
	}

	for (i=0; i < mipi_rd_buf_len; i++) {
		if (i > 0) {
			strcat(buf,",");
		}
		snprintf(temp,sizeof(temp),"%x", mipi_rd_buf[i]);
		strcat(buf, temp);
	}
	ret = strlen(buf);
	mipi_rd_buf_len = 0;

	return ret;
}

static ssize_t sysfs_mipird_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count) {

	struct dsi_display *dsi_display = dev_get_drvdata(dev);
	struct dsi_panel *panel;
	char *buf_ptr,*tok;
	struct dsi_ctrl_state_info *curr_state;
	size_t len;
	int retry = 10, rc = 0;
	u8 cmd;
	long val;

	if (dsi_display == NULL || dsi_display->panel == NULL ||
		buf == NULL || count == 0) {
		pr_err("Wrong argument");
		return -EINVAL;
	}

	do {
	   curr_state = &dsi_display->ctrl[0].ctrl->current_state;

	   if (curr_state->host_initialized &&
	       curr_state->cmd_engine_state == DSI_CTRL_ENGINE_ON) {
	      break;
	   }

	   retry--;
	   mdelay(100);
	   pr_err("sysfs_mipird_write: DSI CMD engine is not ready. Retrying...");

	} while (retry);

	if (!retry) {
	   pr_err("sysfs_mipird_write: DSI CMD engine is not ready. Failed.");
	   return -EBUSY;
	}

	/* Parse cmd */

	buf_ptr = (char *)buf;
	tok = strsep(&buf_ptr, ",");
	if (tok == NULL) {
		pr_err("Missing command");
		return -EINVAL;
	}

	if (tok[0]=='0' && tok[1]=='x') {
		kstrtol(tok, 16, &val);
		cmd = (u8)val;
	} else {
		kstrtol(tok, 10, &val);
		cmd = (u8)val;
	}

	/* Parse the expected data length */
	tok = strsep(&buf_ptr, ",");
	if (tok == NULL) {
		pr_err("Mising size of read");
		return -EINVAL;
	}
	if (tok[0] == '0' && tok[1] == 'x') {
		kstrtol(tok, 16, &val);
		len = (size_t)val;
	} else {
		kstrtol(tok, 10, &val);
		len = (size_t)val;
	}

	if (len > MIPI_RD_BUF_SZ) {
		pr_err("The expected read is lager than buffer");
		return -EINVAL;
	}

	panel = dsi_display->panel;

	mutex_lock(&panel->panel_lock);
	if (!dsi_panel_initialized(panel)) {
		pr_err("Panel not initialized");
		rc = -EINVAL;
		goto error;
	}


	rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clocks, rc=%d\n",
		       dsi_display->name, rc);
		goto error;
	}

	mipi_dsi_dcs_read(&panel->mipi_device, cmd, mipi_rd_buf, len);
	mipi_rd_buf_len = len;
	pr_info("Read %d bytes", mipi_rd_buf_len);


	rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);
	if (rc) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
		       dsi_display->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return count;
}


static DEVICE_ATTR(mipird, 0664,
			sysfs_mipird_read,
			sysfs_mipird_write);

static struct attribute *mipird_fs_attrs[] = {
	&dev_attr_mipird.attr,
	NULL,
};
static struct attribute_group mipird_fs_attrs_group = {
	.attrs = mipird_fs_attrs,
};

/* MIPI RW  */

static ssize_t sysfs_mipiwr_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count) {

	struct dsi_display *dsi_display = dev_get_drvdata(dev);
	struct dsi_panel *panel;
	struct dsi_ctrl_state_info *curr_state;
	int i = 0, retry = 10, rc = 0;
	u8 cmd;
	int len;
	u8 payload[256];
	char *tok, *buf_ptr;
	unsigned long val;


	if (dsi_display == NULL || dsi_display->panel == NULL ||
		buf == NULL || count == 0) {
		pr_err("Wrong argument");
		return -EINVAL;
	}

	do {
	   curr_state = &dsi_display->ctrl[0].ctrl->current_state;

	   if (curr_state->host_initialized &&
	       curr_state->cmd_engine_state == DSI_CTRL_ENGINE_ON) {
	      break;
	   }

	   retry--;
	   mdelay(100);
	   pr_err("sysfs_mipird_write: DSI CMD engine is not ready. Retrying...");

        } while (retry);

	if (!retry) {
	   pr_err("sysfs_mipird_write: DSI CMD engine is not ready. Failed.");
	   return -EBUSY;
	}

	/* Parse cmd */
	buf_ptr = (char *)buf;
	tok = strsep(&buf_ptr, ",");
	if (tok == NULL) {
		pr_err("Missing command");
		return -EINVAL;
	}

	if (tok[0]=='0' && tok[1]=='x') {
		kstrtol(tok, 16, &val);
		cmd = (u8)val;
	} else {
		kstrtol(tok, 10, &val);
		cmd = (u8)val;
	}


    /* Parse payload length */
	tok = strsep(&buf_ptr, ",");
	if (tok == NULL) {
		pr_err("Missing payload length");
		return -EINVAL;
	}

	if (tok[0]=='0' && tok[1]=='x') {
		kstrtol(tok, 16, &val);
		len = (u8)val;
	} else {
		kstrtol(tok, 10, &val);
		len = (u8)val;
	}

	if (len > sizeof(payload)) {
		pr_err("Payload is too big");
		return -EINVAL;
	}


    /* Parse payload */
	if (len != 0) {
		tok = strsep(&buf_ptr, ",");
		while (tok != NULL && i < sizeof(payload)) {
			if (tok[0]=='0' && tok[1]=='x') {
				kstrtol(tok, 16, &val);
				payload[i++]=(u8)val;
			} else {
				kstrtol(tok, 10, &val);
				payload[i++]=(u8)val;
			}
			tok = strsep(&buf_ptr, ",");
		}
		if (len != i) {
			pr_err("Incorrect payload length");
			return -EINVAL;
		}
	}


	if (!dsi_display) {
		pr_err("Invalid display\n");
		return -EINVAL;
	}

	panel = dsi_display->panel;

	mutex_lock(&panel->panel_lock);

    rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
	if (rc) {
		pr_err("[%s] failed to enable DSI core clo	cks, rc=%d\n",
		       dsi_display->name, rc);
		rc = -EIO;
		goto error;
	}

    rc = mipi_dsi_dcs_write(&panel->mipi_device, cmd, payload, len);
	if (rc == len) {
		rc = count;
	}

	if (dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF)) {
		pr_err("[%s] failed to disable DSI core clocks, rc=%d\n",
			   dsi_display->name, rc);
		goto error;
	}

error:
	mutex_unlock(&panel->panel_lock);
	return rc;
}

static DEVICE_ATTR(mipiwr, 0220,
			NULL,
			sysfs_mipiwr_write);

static struct attribute *mipiwr_fs_attrs[] = {
	&dev_attr_mipiwr.attr,
	NULL,
};
static struct attribute_group mipiwr_fs_attrs_group = {
	.attrs = mipiwr_fs_attrs,
};

/* Sysfs changing MIPI response payload size handle */

static ssize_t sysfs_mipi_pack_sz_write(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count) {
    ssize_t rc = 0;
    unsigned long val;
    struct dsi_display *display = dev_get_drvdata(dev);
    struct dsi_panel *panel;

    if (buf == NULL) {
	pr_err("[%s] sysfs_mipi_pack_sz_write buf == NULL\n",
	       display->name);
	return -1;
    }

    if (buf[0] == '0' && buf[1] == 'x') {
	kstrtol(buf, 16, &val);
    } else {
	kstrtol(buf, 10, &val);
    }
    if (val == 0 ) {
	pr_err("[%s] sysfs_mipi_pack_sz_write val == 0\n",
	       display->name);
	return -1;
    }

    panel = display->panel;

    rc = mipi_dsi_set_maximum_return_packet_size(&panel->mipi_device, val);

    return rc;
}

static DEVICE_ATTR(mipi_pack_sz, 0220,
		   NULL,
		   sysfs_mipi_pack_sz_write);

static struct attribute *mipi_pack_sz_fs_attrs[] = {
	&dev_attr_mipi_pack_sz.attr,
	NULL,
};
static struct attribute_group mipi_pack_sz_fs_attrs_group = {
	.attrs = mipi_pack_sz_fs_attrs,
};

int dsi_mipi_inject_sysfs_init(struct dsi_display *display)
{
	int rc = 0;
	struct device *dev = &display->pdev->dev;
        if (display->panel->panel_mode == DSI_OP_CMD_MODE) {
           rc = sysfs_create_group(&dev->kobj,
                           &mipird_fs_attrs_group);
           rc = sysfs_create_group(&dev->kobj,
                           &mipiwr_fs_attrs_group);
           rc = sysfs_create_group(&dev->kobj,
                           &mipi_pack_sz_fs_attrs_group);
        }

        rc = qsync_enable_sysfs_init(display);

	return rc;
}

int dsi_mipi_inject_sysfs_deinit(struct dsi_display *display)
{
	struct device *dev = &display->pdev->dev;

        qsunc_enable_sysfs_deinit(display);

        if (display->panel->panel_mode == DSI_OP_CMD_MODE) {

                sysfs_remove_group(&dev->kobj,
                                   &mipird_fs_attrs_group);
                sysfs_remove_group(&dev->kobj,
                                   &mipiwr_fs_attrs_group);
                sysfs_remove_group(&dev->kobj,
                                   &mipi_pack_sz_fs_attrs_group);
        }

	return 0;
}
