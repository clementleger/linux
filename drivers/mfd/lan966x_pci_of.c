// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) Microchip UNG
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/mfd/core.h>
#include <linux/pci.h>
#include <linux/i2c.h>
#include <linux/iopoll.h>
#include <linux/libfdt.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/machine.h>
#include <linux/property.h>
#include <linux/pci_ids.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>

#include "lan966x_pci_irq.h"

#define DTB_EXTRA_SPACE			200 

#define PCI_DEVICE_ID_MCHP		0x1055
#define PCI_DEVICE_ID_MCHP_LAN966X	0x9660

#define CPU_RESET_PROT_STAT_OFFSET	0x88
#define CPU_TARGET_OFFSET		0xc0000
#define CPU_TARGET_LENGTH		0x10000

#define LAN966X_BAR_CPU_OFFSET		0xe2000000
#define LAN966X_BAR_CPU_SIZE		SZ_32M
#define LAN966X_BAR_AMBA_OFFSET		0xe0000000
#define LAN966X_BAR_AMBA_SIZE		SZ_16M

#define LAN966X_BAR_CPU		0
#define LAN966X_BAR_AMBA	1
#define LAN966X_BAR_COUNT	2

extern char __dtb_lan966x_pci_begin[];
extern char __dtb_lan966x_pci_end[];

struct lan966x_pci {
	struct device *dev;
	struct pci_dev *pci_dev;
	struct of_changeset of_cs;
	int ovcs_id;
};

static int of_bar_remap(struct pci_dev *pci_dev, int bar, u64 offset, u32 val[5])
{
	u64 start, size;
	u32 flags;

	start = pci_resource_start(pci_dev, bar);
	if (!start)
		return -EINVAL;

	flags = pci_resource_flags(pci_dev, bar);
	if (!(flags & IORESOURCE_MEM))
		return -EINVAL;

	/* Bus address */
	val[0] = offset;

	/* PCI bus address */
	val[1] = 0x2 << 24;
	val[2] = start >> 32;
	val[3] = start;

	/* Size */
	size = pci_resource_len(pci_dev, bar);
	val[4] = size;

	return 0;
}

static int lan966x_pci_load_overlay(struct lan966x_pci *data)
{
	u32 dtbo_size = __dtb_lan966x_pci_end - __dtb_lan966x_pci_begin;
	void *dtbo_start = __dtb_lan966x_pci_begin;
	u32 val[LAN966X_BAR_COUNT][5];
	struct device_node *itc_node;
	int ret;

	ret = of_overlay_fdt_apply_to_node(dtbo_start, dtbo_size, &data->ovcs_id,
					   data->dev->of_node);
	if (ret)
		return ret;

	itc_node = of_get_child_by_name(data->dev->of_node, "itc");
	if (!itc_node) {
		of_overlay_remove(&data->ovcs_id);
		return -EINVAL;
	}

	of_bar_remap(data->pci_dev, LAN966X_BAR_CPU, LAN966X_BAR_CPU_OFFSET,
		     val[0]);
	of_bar_remap(data->pci_dev, LAN966X_BAR_AMBA, LAN966X_BAR_AMBA_OFFSET,
		     val[1]);

	of_changeset_init(&data->of_cs);

	of_changeset_add_prop_u32(&data->of_cs, itc_node, "interrupts",
				  data->pci_dev->irq);

	of_changeset_add_prop_u32_array(&data->of_cs, data->dev->of_node, "ranges",
					(const u32 *) val,
					ARRAY_SIZE(val) * ARRAY_SIZE(val[0]));
	of_changeset_apply(&data->of_cs);

	of_node_put(itc_node);

	return ret;
}

static int lan966x_pci_probe(struct pci_dev *pdev,
			      const struct pci_device_id *id)
{
	struct device *dev = &pdev->dev;
	struct lan966x_pci *data;
	int ret;

	if (!dev->of_node) {
		dev_err(dev, "Missing of_node for device");
		return -EINVAL;
	}

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(dev, data);
	data->dev = dev;
	data->pci_dev = pdev;

	ret = lan966x_pci_load_overlay(data);
	if (ret)
		return ret;

	pci_set_master(pdev);

	ret = of_platform_default_populate(dev->of_node, NULL, dev);
	if (ret)
		of_overlay_remove(&data->ovcs_id);

	return ret;
}

static void lan966x_pci_remove(struct pci_dev *pdev)
{
	struct device *dev = &pdev->dev;
	struct lan966x_pci *data = dev_get_drvdata(dev);

	of_changeset_revert(&data->of_cs);
	of_changeset_destroy(&data->of_cs);

	of_overlay_remove(&data->ovcs_id);

	/* of_platform_depopulate() does not honor device refcount and removes
	 * all devices unconditionally which seems clearly wrong. During overlay
	 * removal below, the device refcount are actually dropped and the
	 * devices are removed gracefully.
	 */
	of_node_clear_flag(dev->of_node, OF_POPULATED);
	of_node_clear_flag(dev->of_node, OF_POPULATED_BUS);

	pci_clear_master(pdev);
}

/* PCI device */
static struct pci_device_id lan966x_ids[] = {
	{ PCI_DEVICE(PCI_DEVICE_ID_MCHP, PCI_DEVICE_ID_MCHP_LAN966X) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, lan966x_ids);

static struct pci_driver lan966x_pci_driver = {
	.name = "mchp_lan966x",
	.id_table = lan966x_ids,
	.probe = lan966x_pci_probe,
	.remove = lan966x_pci_remove,
};

static int __init lan966x_pci_driver_init(void)
{
	int ret;

	ret = lan966x_pci_irq_init();
	if (ret)
		return ret;
	
	ret = pci_register_driver(&lan966x_pci_driver);
	if (ret)
		lan966x_pci_irq_exit();
	
	return ret;
}
module_init(lan966x_pci_driver_init);

static void __exit lan966x_pci_driver_exit(void)
{
	pci_unregister_driver(&lan966x_pci_driver);
	lan966x_pci_irq_exit();
}
module_exit(lan966x_pci_driver_exit);

MODULE_DESCRIPTION("Maserati PCI driver");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Clément Léger <clement.leger@bootlin.com>");
