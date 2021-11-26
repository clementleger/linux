// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Microchip lan966x
 *
 * License: Dual MIT/GPL
 * Copyright (c) 2019 Microchip Corporation
 */
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/irqchip.h>
#include <linux/irq.h>
#include <linux/iopoll.h>
#include <linux/mfd/core.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include "lan966x_pci_irq.h"
#include "lan966x_pci_regs_sr.h"

struct lan966x_irq_data {
	struct irq_domain *domain;
	void __iomem *regs;
	int irq;
};

#define LAN966X_NR_IRQ		63

#define LAN_OFFSET_(id, tinst, tcnt,			\
		    gbase, ginst, gcnt, gwidth,		\
		    raddr, rinst, rcnt, rwidth)		\
	((ginst) * gwidth) + raddr + ((rinst * rwidth))
#define LAN_OFFSET(...) LAN_OFFSET_(__VA_ARGS__)

void disp_lan(struct irq_chip_generic *gc)
{
	u32 val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_ENA));
	pr_err("CPU_INTR_ENA: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_ENA1));
	pr_err("CPU_INTR_ENA1: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_ENA2));
	pr_err("CPU_INTR_ENA2: %x\n", val);

	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_STICKY));
	pr_err("CPU_INTR_STICKY: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_STICKY1));
	pr_err("CPU_INTR_STICKY1: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_STICKY2));
	pr_err("CPU_INTR_STICKY2: %x\n", val);

	val = irq_reg_readl(gc, LAN_OFFSET(CPU_DST_INTR_IDENT(0)));
	pr_err("CPU_DST_INTR_IDENT(0): %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_DST_INTR_IDENT1(0)));
	pr_err("CPU_DST_INTR_IDENT1(0): %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_DST_INTR_IDENT2(0)));
	pr_err("CPU_DST_INTR_IDENT1(0): %x\n", val);

	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_RAW));
	pr_err("CPU_INTR_RAW: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_RAW1));
	pr_err("CPU_INTR_RAW1: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_RAW2));
	pr_err("CPU_INTR_RAW2: %x\n", val);

	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_FORCE));
	pr_err("CPU_INTR_FORCE: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_FORCE1));
	pr_err("CPU_INTR_FORCE1: %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_INTR_FORCE2));
	pr_err("CPU_INTR_FORCE2: %x\n", val);

	val = irq_reg_readl(gc, LAN_OFFSET(CPU_DST_INTR_MAP(0)));
	pr_err("CPU_DST_INTR_MAP(0): %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_DST_INTR_MAP1(0)));
	pr_err("CPU_DST_INTR_MAP1(0): %x\n", val);
	val = irq_reg_readl(gc, LAN_OFFSET(CPU_DST_INTR_MAP2(0)));
	pr_err("CPU_DST_INTR_MAP2(0): %x\n", val);
}

static void lan966x_irq_unmask(struct irq_data *data)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(data);
	struct irq_chip_type *ct = irq_data_get_chip_type(data);
	unsigned int mask = data->mask;

	irq_gc_lock(gc);
	irq_reg_writel(gc, mask, gc->chip_types[0].regs.ack);
	*ct->mask_cache |= mask;
	irq_reg_writel(gc, mask, gc->chip_types[0].regs.enable);

	if (gc->chip_types[0].regs.enable == LAN_OFFSET(CPU_INTR_ENA_SET))
		irq_reg_writel(gc, *ct->mask_cache, LAN_OFFSET(CPU_DST_INTR_MAP(0)));
	else
		irq_reg_writel(gc, *ct->mask_cache, LAN_OFFSET(CPU_DST_INTR_MAP1(0)));

	irq_gc_unlock(gc);
}

static void lan966x_irq_mask(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	u32 mask = d->mask;

	irq_gc_lock(gc);
	*ct->mask_cache &= ~mask;
	irq_reg_writel(gc, mask, ct->regs.mask);

	irq_gc_unlock(gc);
}

static int lan966x_irq_handler_domain(struct irq_domain *d,
				       struct irq_chip *chip,
				       struct irq_desc *desc,
				       u32 first_irq)
{
	struct irq_chip_generic *gc = irq_get_domain_generic_chip(d, first_irq);
	u32 reg = irq_reg_readl(gc, gc->chip_types[0].regs.type);
	u32 mask;

	if (!gc->chip_types[0].mask_cache || !reg)
		return 0;

	mask = *gc->chip_types[0].mask_cache;
	reg &= mask;

	chained_irq_enter(chip, desc);
	while (reg) {
		u32 hwirq = __fls(reg);

		generic_handle_irq(irq_find_mapping(d, hwirq + first_irq));
		reg &= ~(BIT(hwirq));
	}
	chained_irq_exit(chip, desc);

	return 1;
}

static void lan966x_irq_handler(struct irq_desc *desc)
{
	struct irq_domain *d = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);

	lan966x_irq_handler_domain(d, chip, desc, 0);
	lan966x_irq_handler_domain(d, chip, desc, 32);
}

static int lan966x_pci_irq_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct lan966x_irq_data *lan966x_irq;
	struct device *dev = &pdev->dev;
	struct irq_chip_generic *gc;
	int ret;

	lan966x_irq = devm_kmalloc(dev, sizeof(*lan966x_irq), GFP_KERNEL);
	if (!lan966x_irq)
		return -ENOMEM;

	lan966x_irq->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(lan966x_irq->regs)) {
		dev_err(dev, "Could not map resource\n");
		return PTR_ERR(lan966x_irq->regs);
	}

	lan966x_irq->domain = irq_domain_add_linear(node, LAN966X_NR_IRQ,
				       &irq_generic_chip_ops, NULL);
	if (!lan966x_irq->domain) {
		dev_err(dev, "unable to add irq domain\n");
		return -EINVAL;
	}

	ret = of_property_read_u32(node, "interrupts", &lan966x_irq->irq);
	if (ret) {
		dev_err(dev, "Failed to get interrupts\n");
		return ret;
	}

	ret = irq_alloc_domain_generic_chips(lan966x_irq->domain, 32,
					     LAN966X_NR_IRQ / 32, "icpu",
					     handle_level_irq, 0, 0, 0);
	if (ret) {
		dev_err(dev, "unable to alloc irq domain gc\n");
		goto err_domain_remove;
	}

	/* Get first domain(0-31) */
	gc = irq_get_domain_generic_chip(lan966x_irq->domain, 0);
	gc->reg_base = lan966x_irq->regs;
	gc->chip_types[0].regs.enable = LAN_OFFSET(CPU_INTR_ENA_SET);
	gc->chip_types[0].regs.type = LAN_OFFSET(CPU_DST_INTR_IDENT(0));
	gc->chip_types[0].regs.ack = LAN_OFFSET(CPU_INTR_STICKY);
	gc->chip_types[0].regs.mask = LAN_OFFSET(CPU_INTR_ENA_CLR);
	gc->chip_types[0].chip.irq_ack = irq_gc_ack_set_bit;
	gc->chip_types[0].chip.irq_mask = lan966x_irq_mask;
	gc->chip_types[0].chip.irq_unmask = lan966x_irq_unmask;

	irq_reg_writel(gc, 0x0, LAN_OFFSET(CPU_INTR_ENA));

	/* Get second domain(32-63) */
	gc = irq_get_domain_generic_chip(lan966x_irq->domain, 32);
	gc->reg_base = lan966x_irq->regs;
	gc->chip_types[0].regs.enable = LAN_OFFSET(CPU_INTR_ENA_SET1);
	gc->chip_types[0].regs.type = LAN_OFFSET(CPU_DST_INTR_IDENT1(0));
	gc->chip_types[0].regs.ack = LAN_OFFSET(CPU_INTR_STICKY1);
	gc->chip_types[0].regs.mask = LAN_OFFSET(CPU_INTR_ENA_CLR1);
	gc->chip_types[0].chip.irq_ack = irq_gc_ack_set_bit;
	gc->chip_types[0].chip.irq_mask = lan966x_irq_mask;
	gc->chip_types[0].chip.irq_unmask = lan966x_irq_unmask;

	irq_reg_writel(gc, 0x0, LAN_OFFSET(CPU_INTR_ENA1));

	irq_set_chained_handler_and_data(lan966x_irq->irq, lan966x_irq_handler,
					 lan966x_irq->domain);

	platform_set_drvdata(pdev, lan966x_irq);

	return 0;

err_domain_remove:
	irq_domain_remove(lan966x_irq->domain);

	return ret;
}

static int lan966x_pci_irq_remove(struct platform_device *pdev)
{
	struct lan966x_irq_data *lan966x_irq = platform_get_drvdata(pdev);
	struct irq_chip_generic *gc = lan966x_irq->domain->gc->gc[0];
	int i;

	irq_reg_writel(gc, 0x0, LAN_OFFSET(CPU_INTR_ENA));
	irq_reg_writel(gc, 0x0, LAN_OFFSET(CPU_INTR_ENA1));

	irq_set_chained_handler_and_data(lan966x_irq->irq, NULL, NULL);
	for (i = 0; i < LAN966X_NR_IRQ; i++)
		irq_dispose_mapping(irq_find_mapping(lan966x_irq->domain, i));

	irq_domain_remove(lan966x_irq->domain);

	irq_remove_generic_chip(lan966x_irq->domain->gc->gc[0], 0x17e00, 0, 0);
	irq_remove_generic_chip(lan966x_irq->domain->gc->gc[1], 0x1f0000, 0, 0);
	kfree(lan966x_irq->domain->gc);

	return 0;
}

static const struct of_device_id lan966xc_pci_irq_of_match[] = {
	{
		.compatible = "microchip,lan966x-itc",
	},
	{ }
};
MODULE_DEVICE_TABLE(of, lan966xc_pci_irq_of_match);

static struct platform_driver lan966xc_pci_irq_driver = {
	.probe = lan966x_pci_irq_probe,
	.remove = lan966x_pci_irq_remove,
	.driver = {
		.name = "lan966x-pci-irq",
		.of_match_table = lan966xc_pci_irq_of_match,
	},
};

int lan966x_pci_irq_init(void)
{
	return platform_driver_register(&lan966xc_pci_irq_driver);
}

void lan966x_pci_irq_exit(void)
{
	platform_driver_unregister(&lan966xc_pci_irq_driver);
}
