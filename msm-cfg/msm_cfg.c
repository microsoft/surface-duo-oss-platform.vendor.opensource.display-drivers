// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/component.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/of_device.h>

static unsigned int cfg_sel;
module_param_named(cfg_sel, cfg_sel, int, 0600);
MODULE_PARM_DESC(cfg_sel,
	"msm_cfg.cfg_sel=<N> where <N> is the sub-cfg reg address to be selected");

static int msm_cfg_bind(struct device *dev, struct device *master,
		void *data)
{
	void *drvdata;
	int rc;

	drvdata = dev_get_drvdata(master);

	dev_set_drvdata(dev, drvdata);

	rc = component_bind_all(dev, drvdata);
	if (rc) {
		pr_err("failed to bind all\n");
		return rc;
	}

	return 0;
}

static void msm_cfg_unbind(struct device *dev, struct device *master,
		void *data)
{
}

static const struct component_ops msm_cfg_comp_ops = {
	.bind = msm_cfg_bind,
	.unbind = msm_cfg_unbind,
};

static int msm_cfg_master_bind(struct device *dev)
{
	return 0;
}

static void msm_cfg_master_unbind(struct device *dev)
{
}

static const struct component_master_ops msm_cfg_master_ops = {
	.bind = msm_cfg_master_bind,
	.unbind = msm_cfg_master_unbind,
};

static int compare_of(struct device *dev, void *data)
{
	return dev->of_node == data;
}

static int msm_cfg_probe(struct platform_device *pdev)
{
	struct component_match *match = NULL;
	struct device *dev = &pdev->dev;
	struct device_node *node, *cfg_node = NULL;
	u32 reg;
	int rc, i;

	for_each_child_of_node(dev->of_node, node) {
		rc = of_property_read_u32(node, "reg", &reg);
		if (rc) {
			pr_err("failed to get sub cfg address\n");
			continue;
		}

		if (reg == cfg_sel) {
			cfg_node = node;
			break;
		}
	}

	if (!cfg_node) {
		pr_err("failed to find cfg node\n");
		return -ENODEV;
	}

	for (i = 0; ; i++) {
		node = of_parse_phandle(cfg_node, "connectors", i);
		if (!node)
			break;

		component_match_add(&pdev->dev, &match, compare_of, node);
	}

	if (!match) {
		pr_err("failed to find connector\n");
		return -ENODEV;
	}

	for_each_child_of_node(cfg_node, node) {
		/* create all sub devices */
		of_platform_device_create(node, NULL, NULL);
	}

	rc = component_master_add_with_match(&pdev->dev,
			&msm_cfg_master_ops, match);
	if (rc) {
		pr_err("failed to add match master\n");
		return rc;
	}

	rc = component_add(dev, &msm_cfg_comp_ops);
	if (rc) {
		pr_err("failed to add component\n");
		return rc;
	}

	return 0;
}

static int msm_cfg_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,sde-cfg" },
	{}
};
MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver msm_cfg_platform_driver = {
	.probe      = msm_cfg_probe,
	.remove     = msm_cfg_remove,
	.driver     = {
		.name   = "msm_cfg",
		.of_match_table = dt_match,
	},
};

static int __init msm_cfg_register(void)
{
	return platform_driver_register(&msm_cfg_platform_driver);
}

static void __exit msm_cfg_unregister(void)
{
	platform_driver_unregister(&msm_cfg_platform_driver);
}

module_init(msm_cfg_register);
module_exit(msm_cfg_unregister);

MODULE_DESCRIPTION("MSM CFG Driver");
MODULE_LICENSE("GPL v2");
