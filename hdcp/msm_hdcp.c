/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018,2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[msm-hdcp] %s: " fmt, __func__

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_platform.h>

#include "msm_hdcp.h"

#define CLASS_NAME "hdcp"
#define DRIVER_NAME "msm_hdcp"

static struct class *class;

static DEFINE_MUTEX(master_mutex);

struct msm_hdcp {
	struct platform_device *pdev;
	dev_t dev_num;
	struct cdev cdev;
	struct device *device;
	struct HDCP_V2V1_MSG_TOPOLOGY cached_tp;
	u32 tp_msgid;
	void *client_ctx;
	void (*cb)(void *ctx, u8 data);
	int state;
	int version;
	u8 min_enc_level;
	u32 cell_idx;
	struct msm_hdcp *master_hdcp;
	struct list_head head;
	struct list_head slave_list;
};

void msm_hdcp_register_cb(struct device *dev, void *ctx,
	void (*cb)(void *ctx, u8 data))
{
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return;
	}

	hdcp->cb = cb;
	hdcp->client_ctx = ctx;
}

void msm_hdcp_notify_status(struct device *dev,
		struct msm_hdcp_status *status)
{
	char *envp[2];
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return;
	}
	if ((status->state != hdcp->state) ||
			(status->version != hdcp->version) ||
			(status->min_enc_level != hdcp->min_enc_level)) {
		hdcp->state = status->state;
		hdcp->version = status->version;
		hdcp->min_enc_level = status->min_enc_level;

		envp[0] = "HDCP_UPDATE=1";
		envp[1] = NULL;
		kobject_uevent_env(&hdcp->device->kobj,
				KOBJ_CHANGE, envp);
	}
}

void msm_hdcp_notify_topology(struct device *dev)
{
	char *envp[4];
	char tp[SZ_16];
	char ver[SZ_16];
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return;
	}

	snprintf(tp, SZ_16, "%d", DOWN_CHECK_TOPOLOGY);
	snprintf(ver, SZ_16, "%d", HDCP_V1_TX);

	envp[0] = "HDCP_MGR_EVENT=MSG_READY";
	envp[1] = tp;
	envp[2] = ver;
	envp[3] = NULL;

	kobject_uevent_env(&hdcp->device->kobj, KOBJ_CHANGE, envp);
}

void msm_hdcp_cache_repeater_topology(struct device *dev,
			struct HDCP_V2V1_MSG_TOPOLOGY *tp)
{
	struct msm_hdcp *hdcp = NULL;

	if (!dev || !tp) {
		pr_err("invalid input\n");
		return;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return;
	}

	memcpy(&hdcp->cached_tp, tp,
		   sizeof(struct HDCP_V2V1_MSG_TOPOLOGY));
}

static struct msm_hdcp *msm_hdcp_get_master_dev(struct device_node *of_node)
{
	struct device_node *node;
	struct msm_hdcp *master_hdcp;
	struct platform_device *pdev;

	node = of_parse_phandle(of_node, "qcom,msm-hdcp-master", 0);
	if (!node) {
		// This is master msm initializa the slave list
		return NULL;
	}
	pdev = of_find_device_by_node(node);
	if (!pdev) {
		// defer the  module initialization
		pr_err("couldn't find msm-hdcp pdev defer probe\n");
		return ERR_PTR(-EPROBE_DEFER);
	}

	master_hdcp = dev_get_drvdata(&pdev->dev);
	if (!master_hdcp) {
		pr_err("invalid driver pointer\n");
		return ERR_PTR(-EPROBE_DEFER);
	}
	return master_hdcp;
}

static int msm_hdcp_add_master_dev(struct device_node *of_node,
			struct msm_hdcp *hdcp)
{
	hdcp->master_hdcp = msm_hdcp_get_master_dev(of_node);

	INIT_LIST_HEAD(&hdcp->slave_list);

	if (hdcp->master_hdcp) {
		mutex_lock(&master_mutex);
		list_add(&hdcp->head, &hdcp->master_hdcp->slave_list);
		mutex_unlock(&master_mutex);
	}
	return 0;
}

static void msm_hdcp_del_master_dev(struct device_node *of_node,
		struct msm_hdcp *hdcp)
{
	struct msm_hdcp *hdcp_node;

	mutex_lock(&master_mutex);

	if (!hdcp->master_hdcp) {
		/*Master msm hdcp is getting removed delete the slave list */
		list_for_each_entry(hdcp_node, &hdcp->slave_list, head) {
			list_del_init(&hdcp_node->head);
			hdcp_node->master_hdcp = NULL;
		}
	} else {
		list_del(&hdcp->head);
	}

	mutex_unlock(&master_mutex);
}

static ssize_t tp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	switch (hdcp->tp_msgid) {
	case DOWN_CHECK_TOPOLOGY:
	case DOWN_REQUEST_TOPOLOGY:
		buf[MSG_ID_IDX]   = hdcp->tp_msgid;
		buf[RET_CODE_IDX] = HDCP_AUTHED;
		ret = HEADER_LEN;

		memcpy(buf + HEADER_LEN, &hdcp->cached_tp,
			   sizeof(struct HDCP_V2V1_MSG_TOPOLOGY));

		ret += sizeof(struct HDCP_V2V1_MSG_TOPOLOGY);

		/* clear the flag once data is read back to user space*/
		hdcp->tp_msgid = -1;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t tp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int msgid = 0;
	ssize_t ret = count;
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	msgid = buf[0];

	switch (msgid) {
	case DOWN_CHECK_TOPOLOGY:
	case DOWN_REQUEST_TOPOLOGY:
		hdcp->tp_msgid = msgid;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static ssize_t msm_hdcp_2x_sysfs_wta_min_level_change(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int rc;
	int min_enc_lvl;
	ssize_t ret = count;
	struct msm_hdcp *hdcp = NULL;
	struct msm_hdcp *hdcp_node;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	rc = kstrtoint(buf, 10, &min_enc_lvl);
	if (rc) {
		pr_err("kstrtoint failed. rc=%d\n", rc);
		return -EINVAL;
	}

	if (hdcp->cb && hdcp->client_ctx)
		hdcp->cb(hdcp->client_ctx, min_enc_lvl);

	mutex_lock(&master_mutex);
	list_for_each_entry(hdcp_node, &hdcp->slave_list, head) {
		if (hdcp_node->cb && hdcp_node->client_ctx)
			hdcp_node->cb(hdcp_node->client_ctx, min_enc_lvl);
	}
	mutex_unlock(&master_mutex);
	return ret;
}

static ssize_t msm_hdcp_sysfs_hdcp_state(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}
	return scnprintf(buf, PAGE_SIZE, "%zu\n", hdcp->state);
}

static ssize_t msm_hdcp_sysfs_hdcp_version(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	return scnprintf(buf, PAGE_SIZE, "%zu\n", hdcp->version);
}

static ssize_t hdcp_mel_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct msm_hdcp *hdcp = NULL;

	if (!dev) {
		pr_err("invalid device pointer\n");
		return -ENODEV;
	}

	hdcp = dev_get_drvdata(dev);
	if (!hdcp) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}
	return scnprintf(buf, PAGE_SIZE, "%zu\n", hdcp->min_enc_level);
}

static DEVICE_ATTR_RO(hdcp_mel);

static DEVICE_ATTR_RW(tp);

static DEVICE_ATTR(min_level_change, 0200, NULL,
	msm_hdcp_2x_sysfs_wta_min_level_change);

static DEVICE_ATTR(hdcp_state, 0444, msm_hdcp_sysfs_hdcp_state,
	NULL);

static DEVICE_ATTR(hdcp_version, 0444, msm_hdcp_sysfs_hdcp_version,
	NULL);


static struct attribute *msm_hdcp_fs_attrs[] = {
	&dev_attr_tp.attr,
	&dev_attr_min_level_change.attr,
	&dev_attr_hdcp_state.attr,
	&dev_attr_hdcp_version.attr,
	&dev_attr_hdcp_mel.attr,
	NULL
};

static struct attribute_group msm_hdcp_fs_attr_group = {
	.attrs = msm_hdcp_fs_attrs
};

static int msm_hdcp_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int msm_hdcp_close(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations msm_hdcp_fops = {
	.owner = THIS_MODULE,
	.open = msm_hdcp_open,
	.release = msm_hdcp_close,
};

static const struct of_device_id msm_hdcp_dt_match[] = {
	{ .compatible = "qcom,msm-hdcp",},
	{}
};

MODULE_DEVICE_TABLE(of, msm_hdcp_dt_match);

static int msm_hdcp_probe(struct platform_device *pdev)
{
	int ret;
	struct msm_hdcp *hdcp;
	char driver_name[10];
	struct device_node *of_node = pdev->dev.of_node;
	hdcp = devm_kzalloc(&pdev->dev, sizeof(struct msm_hdcp), GFP_KERNEL);
	if (!hdcp)
		return -ENOMEM;

	hdcp->pdev = pdev;
	hdcp->state = 0;
	hdcp->version = 0;
	hdcp->min_enc_level = 0;
	platform_set_drvdata(pdev, hdcp);

	of_property_read_u32(of_node, "cell-index", &hdcp->cell_idx);

	if (hdcp->cell_idx)
		snprintf(driver_name, sizeof(driver_name), "%s%d",
				DRIVER_NAME, hdcp->cell_idx);
	else
		snprintf(driver_name, sizeof(driver_name), "%s", DRIVER_NAME);

	ret = alloc_chrdev_region(&hdcp->dev_num, 0, 1, driver_name);
	if (ret  < 0) {
		pr_err("alloc_chrdev_region failed ret = %d\n", ret);
		goto error_get_dev_num;
	}

	hdcp->device = device_create(class, NULL,
		hdcp->dev_num, hdcp, driver_name);
	if (IS_ERR(hdcp->device)) {
		ret = PTR_ERR(hdcp->device);
		pr_err("device_create failed %d\n", ret);
		goto error_class_device_create;
	}

	cdev_init(&hdcp->cdev, &msm_hdcp_fops);
	ret = cdev_add(&hdcp->cdev, MKDEV(MAJOR(hdcp->dev_num), 0), 1);
	if (ret < 0) {
		pr_err("cdev_add failed %d\n", ret);
		goto error_cdev_add;
	}

	ret = sysfs_create_group(&hdcp->device->kobj, &msm_hdcp_fs_attr_group);
	if (ret)
		pr_err("unable to register msm_hdcp sysfs nodes\n");

	ret = msm_hdcp_add_master_dev(of_node, hdcp);
	if (ret < 0) {
		pr_err("msm hdcp add master failed\n");
		goto error_cdev_add;
	}

	return 0;
error_cdev_add:
	device_destroy(class, hdcp->dev_num);
error_class_device_create:
	unregister_chrdev_region(hdcp->dev_num, 1);
error_get_dev_num:
	devm_kfree(&pdev->dev, hdcp);
	hdcp = NULL;
	return ret;
}

static int msm_hdcp_remove(struct platform_device *pdev)
{
	struct msm_hdcp *hdcp;
	struct device_node *of_node = pdev->dev.of_node;

	hdcp = platform_get_drvdata(pdev);
	if (!hdcp)
		return -ENODEV;

	sysfs_remove_group(&hdcp->device->kobj,
	&msm_hdcp_fs_attr_group);
	cdev_del(&hdcp->cdev);
	device_destroy(class, hdcp->dev_num);

	unregister_chrdev_region(hdcp->dev_num, 1);

	msm_hdcp_del_master_dev(of_node, hdcp);
	devm_kfree(&pdev->dev, hdcp);
	hdcp = NULL;
	return 0;
}

static struct platform_driver msm_hdcp_driver = {
	.probe = msm_hdcp_probe,
	.remove = msm_hdcp_remove,
	.driver = {
		.name = "msm_hdcp",
		.of_match_table = msm_hdcp_dt_match,
		.pm = NULL,
	}
};

void __init msm_hdcp_register(void)
{
	int ret;

	class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(class)) {
		ret = PTR_ERR(class);
		pr_err("couldn't create class rc = %d\n", ret);
	}

	platform_driver_register(&msm_hdcp_driver);
}

void __exit msm_hdcp_unregister(void)
{
	class_destroy(class);
	platform_driver_unregister(&msm_hdcp_driver);
}
