// SPDX-License-Identifier: GPL-2.0
/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2018 Kalray Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/io.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/of_irq.h>
#include <linux/cpumask.h>
#include <linux/interrupt.h>
#include <linux/cpuhotplug.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>

#define IPI_INTERRUPT_OFFSET	0x0
#define IPI_MASK_OFFSET		0x20

/*
 * IPI controller can signal RM and PE0 -> 15
 * In order to restrict that to the PE, write the corresponding mask
 */
#define KVX_IPI_CPU_MASK	(~0xFFFF)

struct kvx_ipi_ctrl {
	void __iomem *regs;
	unsigned int ipi_irq;
};

static struct kvx_ipi_ctrl kvx_ipi_controller;

/**
 * @kvx_pwr_ctrl_cpu_poweron Wakeup a cpu
 *
 * cpu: cpu to wakeup
 */
void kvx_ipi_send(const struct cpumask *mask)
{
	const unsigned long *maskb = cpumask_bits(mask);

	WARN_ON(*maskb & KVX_IPI_CPU_MASK);
	writel(*maskb, kvx_ipi_controller.regs + IPI_INTERRUPT_OFFSET);
}

static int kvx_ipi_starting_cpu(unsigned int cpu)
{
	enable_percpu_irq(kvx_ipi_controller.ipi_irq, IRQ_TYPE_NONE);

	return 0;
}

static int kvx_ipi_dying_cpu(unsigned int cpu)
{
	disable_percpu_irq(kvx_ipi_controller.ipi_irq);

	return 0;
}

int __init kvx_ipi_ctrl_probe(irqreturn_t (*ipi_irq_handler)(int, void *))
{
	struct device_node *np;
	int ret;
	unsigned int ipi_irq;
	void __iomem *ipi_base;

	np = of_find_compatible_node(NULL, NULL, "kalray,kvx-ipi-ctrl");
	BUG_ON(!np);

	ipi_base = of_iomap(np, 0);
	BUG_ON(!ipi_base);

	kvx_ipi_controller.regs = ipi_base;

	/* Init mask for interrupts to PE0 -> PE15 */
	writel(KVX_IPI_CPU_MASK, kvx_ipi_controller.regs + IPI_MASK_OFFSET);

	ipi_irq = irq_of_parse_and_map(np, 0);
	if (!ipi_irq) {
		pr_err("Failed to parse irq: %d\n", ipi_irq);
		return -EINVAL;
	}

	ret = request_percpu_irq(ipi_irq, ipi_irq_handler,
						"kvx_ipi", &kvx_ipi_controller);
	if (ret) {
		pr_err("can't register interrupt %d (%d)\n",
						ipi_irq, ret);
		return ret;
	}
	kvx_ipi_controller.ipi_irq = ipi_irq;

	ret = cpuhp_setup_state(CPUHP_AP_ONLINE_DYN,
				"kvx/ipi:online",
				kvx_ipi_starting_cpu,
				kvx_ipi_dying_cpu);
	if (ret < 0) {
		pr_err("Failed to setup hotplug state");
		return ret;
	}

	pr_info("controller probed\n");

	return 0;
}
