/* SPDX-License-Identifier: (GPL-2.0 OR MIT) */
/*
 * Microchip LAN966x PCI irqchip driver
 */

#ifndef MASERATI_PCI_IRQ_H
#define MASERATI_PCI_IRQ_H

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/platform_device.h>

int lan966x_pci_irq_init(void);
void lan966x_pci_irq_exit(void);

#endif
