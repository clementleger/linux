// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/*
 * LAN966x SoCs pinctrl driver
 *
 * Copyright (c) 2021 Microchip Inc.
 *
 * Author: Kavyasree Kotagiri <kavyasree.kotagiri@microchip.com>
 */

#include <linux/gpio/driver.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include "core.h"
#include "pinconf.h"
#include "pinmux.h"

#define lan966x_clrsetbits(addr, clear, set) \
	writel((readl(addr) & ~(clear)) | (set), (addr))

/* PINCONFIG bits */
enum {
	PINCONF_BIAS,
	PINCONF_SCHMITT,
	PINCONF_DRIVE_STRENGTH,
};

#define BIAS_PD_BIT BIT(4)
#define BIAS_PU_BIT BIT(3)
#define BIAS_BITS   (BIAS_PD_BIT|BIAS_PU_BIT)
#define SCHMITT_BIT BIT(2)
#define DRIVE_BITS  GENMASK(1, 0)

/* GPIO standard registers */
#define LAN966x_GPIO_OUT_SET	0x0
#define LAN966x_GPIO_OUT_CLR	0x4
#define LAN966x_GPIO_OUT	0x8
#define LAN966x_GPIO_IN		0xc
#define LAN966x_GPIO_OE		0x10
#define LAN966x_GPIO_INTR	0x14
#define LAN966x_GPIO_INTR_ENA	0x18
#define LAN966x_GPIO_INTR_IDENT	0x1c
#define LAN966x_GPIO_ALT0	0x20
#define LAN966x_GPIO_ALT1	0x24
#define LAN966x_GPIO_SD_MAP	0x28

#define LAN966x_FUNC_PER_PIN	8

enum {
	FUNC_CAN0_a,
	FUNC_CAN0_b,
	FUNC_CAN1,
	FUNC_NONE,
	FUNC_FC0_a,
	FUNC_FC0_b,
	FUNC_FC0_c,
	FUNC_FC1_a,
	FUNC_FC1_b,
	FUNC_FC1_c,
	FUNC_FC2_a,
	FUNC_FC2_b,
	FUNC_FC3_a,
	FUNC_FC3_b,
	FUNC_FC3_c,
	FUNC_FC4_a,
	FUNC_FC4_b,
	FUNC_FC4_c,
	FUNC_FC_SHRD0,
	FUNC_FC_SHRD1,
	FUNC_FC_SHRD2,
	FUNC_FC_SHRD3,
	FUNC_FC_SHRD4,
	FUNC_FC_SHRD5,
	FUNC_FC_SHRD6,
	FUNC_FC_SHRD7,
	FUNC_FC_SHRD8,
	FUNC_FC_SHRD9,
	FUNC_FC_SHRD10,
	FUNC_FC_SHRD11,
	FUNC_FC_SHRD12,
	FUNC_FC_SHRD13,
	FUNC_FC_SHRD14,
	FUNC_FC_SHRD15,
	FUNC_FC_SHRD16,
	FUNC_FC_SHRD17,
	FUNC_FC_SHRD18,
	FUNC_FC_SHRD19,
	FUNC_FC_SHRD20,
	FUNC_GPIO,
	FUNC_IB_TRG_a,
	FUNC_IB_TRG_b,
	FUNC_IB_TRG_c,
	FUNC_IRQ_IN_a,
	FUNC_IRQ_IN_b,
	FUNC_IRQ_IN_c,
	FUNC_IRQ_OUT_a,
	FUNC_IRQ_OUT_b,
	FUNC_IRQ_OUT_c,
	FUNC_MIIM_a,
	FUNC_MIIM_b,
	FUNC_MIIM_c,
	FUNC_MIIM_Sa,
	FUNC_MIIM_Sb,
	FUNC_OB_TRG,
	FUNC_OB_TRG_a,
	FUNC_OB_TRG_b,
	FUNC_PTPSYNC_1,
	FUNC_PTPSYNC_2,
	FUNC_PTPSYNC_3,
	FUNC_PTPSYNC_4,
	FUNC_PTPSYNC_5,
	FUNC_PTPSYNC_6,
	FUNC_PTPSYNC_7,
	FUNC_QSPI1,
	FUNC_QSPI2,
	FUNC_R,
	FUNC_RECO_a,
	FUNC_RECO_b,
	FUNC_SD,
	FUNC_SFP_SD,
	FUNC_SGPIO,
	FUNC_SGPIO_a,
	FUNC_SGPIO_b,
	FUNC_TACHO_a,
	FUNC_TACHO_b,
	FUNC_TWI_SLC_GATE,
	FUNC_TWI_SLC_GATE_AD,
	FUNC_USB_H_a,
	FUNC_USB_H_b,
	FUNC_USB_H_c,
	FUNC_USB_S_a,
	FUNC_USB_S_b,
	FUNC_USB_S_c,
	FUNC_EMMC,
	FUNC_EMMC_SD,
	FUNC_MAX
};

static const char *const lan966x_function_names[] = {
	[FUNC_CAN0_a]		= "can0_a",
	[FUNC_CAN0_b]		= "can0_b",
	[FUNC_CAN1]		= "can1",
	[FUNC_NONE]		= "none",
	[FUNC_FC0_a]            = "fc0_a",
	[FUNC_FC0_b]            = "fc0_b",
	[FUNC_FC0_c]            = "fc0_c",
	[FUNC_FC1_a]            = "fc1_a",
	[FUNC_FC1_b]            = "fc1_b",
	[FUNC_FC1_c]            = "fc1_c",
	[FUNC_FC2_a]            = "fc2_a",
	[FUNC_FC2_b]            = "fc2_b",
	[FUNC_FC3_a]            = "fc3_a",
	[FUNC_FC3_b]            = "fc3_b",
	[FUNC_FC3_c]            = "fc3_c",
	[FUNC_FC4_a]            = "fc4_a",
	[FUNC_FC4_b]            = "fc4_b",
	[FUNC_FC4_c]            = "fc4_c",
	[FUNC_FC_SHRD0]         = "fc_shrd0",
	[FUNC_FC_SHRD1]         = "fc_shrd1",
	[FUNC_FC_SHRD2]         = "fc_shrd2",
	[FUNC_FC_SHRD3]         = "fc_shrd3",
	[FUNC_FC_SHRD4]         = "fc_shrd4",
	[FUNC_FC_SHRD5]         = "fc_shrd5",
	[FUNC_FC_SHRD6]         = "fc_shrd6",
	[FUNC_FC_SHRD7]         = "fc_shrd7",
	[FUNC_FC_SHRD8]         = "fc_shrd8",
	[FUNC_FC_SHRD9]         = "fc_shrd9",
	[FUNC_FC_SHRD10]        = "fc_shrd10",
	[FUNC_FC_SHRD11]        = "fc_shrd11",
	[FUNC_FC_SHRD12]        = "fc_shrd12",
	[FUNC_FC_SHRD13]        = "fc_shrd13",
	[FUNC_FC_SHRD14]        = "fc_shrd14",
	[FUNC_FC_SHRD15]        = "fc_shrd15",
	[FUNC_FC_SHRD16]        = "fc_shrd16",
	[FUNC_FC_SHRD17]        = "fc_shrd17",
	[FUNC_FC_SHRD18]        = "fc_shrd18",
	[FUNC_FC_SHRD19]        = "fc_shrd19",
	[FUNC_FC_SHRD20]        = "fc_shrd20",
	[FUNC_GPIO]		= "gpio",
	[FUNC_IB_TRG_a]       = "ib_trig_a",
	[FUNC_IB_TRG_b]       = "ib_trig_b",
	[FUNC_IB_TRG_c]       = "ib_trig_c",
	[FUNC_IRQ_IN_a]         = "irq_in_a",
	[FUNC_IRQ_IN_b]         = "irq_in_b",
	[FUNC_IRQ_IN_c]         = "irq_in_c",
	[FUNC_IRQ_OUT_a]        = "irq_out_a",
	[FUNC_IRQ_OUT_b]        = "irq_out_b",
	[FUNC_IRQ_OUT_c]        = "irq_out_c",
	[FUNC_MIIM_a]		= "miim_a",
	[FUNC_MIIM_b]		= "miim_b",
	[FUNC_MIIM_c]		= "miim_c",
	[FUNC_MIIM_Sa]		= "miim_slave_a",
	[FUNC_MIIM_Sb]		= "miim_slave_b",
	[FUNC_OB_TRG]          = "ob_trig",
	[FUNC_OB_TRG_a]        = "ob_trig_a",
	[FUNC_OB_TRG_b]        = "ob_trig_b",
	[FUNC_PTPSYNC_1]	= "ptpsync_1",
	[FUNC_PTPSYNC_2]	= "ptpsync_2",
	[FUNC_PTPSYNC_3]	= "ptpsync_3",
	[FUNC_PTPSYNC_4]	= "ptpsync_4",
	[FUNC_PTPSYNC_5]	= "ptpsync_5",
	[FUNC_PTPSYNC_6]	= "ptpsync_6",
	[FUNC_PTPSYNC_7]	= "ptpsync_7",
	[FUNC_QSPI1]		= "qspi1",
	[FUNC_QSPI2]		= "qspi2",
	[FUNC_R]		= "reserved",
	[FUNC_RECO_a]           = "reco_a",
	[FUNC_RECO_b]           = "reco_b",
	[FUNC_SD]		= "sd",
	[FUNC_SFP_SD]		= "sfp_sd",
	[FUNC_SGPIO]		= "sgpio",
	[FUNC_SGPIO_a]          = "sgpio_a",
	[FUNC_SGPIO_b]          = "sgpio_b",
	[FUNC_TACHO_a]          = "tacho_a",
	[FUNC_TACHO_b]          = "tacho_b",
	[FUNC_TWI_SLC_GATE]     = "twi_slc_gate",
	[FUNC_TWI_SLC_GATE_AD]  = "twi_slc_gate_ad",
	[FUNC_USB_H_a]          = "usb_host_a",
	[FUNC_USB_H_b]          = "usb_host_b",
	[FUNC_USB_H_c]          = "usb_host_c",
	[FUNC_USB_S_a]          = "usb_slave_a",
	[FUNC_USB_S_b]          = "usb_slave_b",
	[FUNC_USB_S_c]          = "usb_slave_c",
	[FUNC_EMMC]		= "emmc",
	[FUNC_EMMC_SD]		= "emmc_sd",
};

struct lan966x_pmx_func {
	const char **groups;
	unsigned int ngroups;
};

struct lan966x_pin_caps {
	unsigned int pin;
	unsigned char functions[LAN966x_FUNC_PER_PIN];
};

struct lan966x_pinctrl {
	struct device *dev;
	struct pinctrl_dev *pctl;
	struct gpio_chip gpio_chip;
	struct regmap *map;
	void __iomem *pincfg;
	struct pinctrl_desc *desc;
	struct lan966x_pmx_func func[FUNC_MAX];
	u8 stride;
};

#define LAN966x_P(p, f0, f1, f2, f3, f4, f5, f6, f7)          \
static struct lan966x_pin_caps lan966x_pin_##p = {             \
	.pin = p,                                              \
	.functions = {                                         \
		FUNC_##f0, FUNC_##f1, FUNC_##f2,               \
		FUNC_##f3, FUNC_##f4, FUNC_##f5,               \
		FUNC_##f6, FUNC_##f7                           \
	},                                                     \
}

/* Pinmuxing table taken from data sheet */
/*        Pin   FUNC0    FUNC1     FUNC2      FUNC3     FUNC4     FUNC5      FUNC6    FUNC7 */
LAN966x_P(0,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(1,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(2,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(3,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(4,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(5,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(6,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(7,    GPIO,    NONE,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(8,    GPIO,   FC0_a,  USB_H_b,      NONE,  USB_S_b,     NONE,      NONE,        R);
LAN966x_P(9,    GPIO,   FC0_a,  USB_H_b,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(10,   GPIO,   FC0_a,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(11,   GPIO,   FC1_a,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(12,   GPIO,   FC1_a,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(13,   GPIO,   FC1_a,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(14,   GPIO,   FC2_a,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(15,   GPIO,   FC2_a,     NONE,      NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(16,   GPIO,   FC2_a, IB_TRG_a,      NONE, OB_TRG_a, IRQ_IN_c, IRQ_OUT_c,        R);
LAN966x_P(17,   GPIO,   FC3_a, IB_TRG_a,      NONE, OB_TRG_a, IRQ_IN_c, IRQ_OUT_c,        R);
LAN966x_P(18,   GPIO,   FC3_a, IB_TRG_a,      NONE, OB_TRG_a, IRQ_IN_c, IRQ_OUT_c,        R);
LAN966x_P(19,   GPIO,   FC3_a, IB_TRG_a,      NONE, OB_TRG_a, IRQ_IN_c, IRQ_OUT_c,        R);
LAN966x_P(20,   GPIO,   FC4_a, IB_TRG_a,      NONE, OB_TRG_a, IRQ_IN_c,      NONE,        R);
LAN966x_P(21,   GPIO,   FC4_a,     NONE,      NONE, OB_TRG_a,     NONE,      NONE,        R);
LAN966x_P(22,   GPIO,   FC4_a,     NONE,      NONE, OB_TRG_a,     NONE,      NONE,        R);
LAN966x_P(23,   GPIO,    NONE,     NONE,      NONE, OB_TRG_a,     NONE,      NONE,        R);
LAN966x_P(24,   GPIO,   FC0_b, IB_TRG_a,   USB_H_c, OB_TRG_a, IRQ_IN_c,   TACHO_a,        R);
LAN966x_P(25,   GPIO,   FC0_b, IB_TRG_a,   USB_H_c, OB_TRG_a, IRQ_OUT_c,   SFP_SD,        R);
LAN966x_P(26,   GPIO,   FC0_b, IB_TRG_a,   USB_S_c, OB_TRG_a,   CAN0_a,    SFP_SD,        R);
LAN966x_P(27,   GPIO,    NONE,     NONE,      NONE, OB_TRG_a,   CAN0_a,      NONE,        R);
LAN966x_P(28,   GPIO,  MIIM_a,     NONE,      NONE, OB_TRG_a, IRQ_OUT_c,   SFP_SD,        R);
LAN966x_P(29,   GPIO,  MIIM_a,     NONE,      NONE, OB_TRG_a,     NONE,      NONE,        R);
LAN966x_P(30,   GPIO,   FC3_c,     CAN1,      NONE,   OB_TRG,   RECO_b,      NONE,        R);
LAN966x_P(31,   GPIO,   FC3_c,     CAN1,      NONE,   OB_TRG,   RECO_b,      NONE,        R);
LAN966x_P(32,   GPIO,   FC3_c,     NONE,   SGPIO_a,     NONE,  MIIM_Sa,      NONE,        R);
LAN966x_P(33,   GPIO,   FC1_b,     NONE,   SGPIO_a,     NONE,  MIIM_Sa,    MIIM_b,        R);
LAN966x_P(34,   GPIO,   FC1_b,     NONE,   SGPIO_a,     NONE,  MIIM_Sa,    MIIM_b,        R);
LAN966x_P(35,   GPIO,   FC1_b,     NONE,   SGPIO_a,   CAN0_b,     NONE,      NONE,        R);
LAN966x_P(36,   GPIO,    NONE,  PTPSYNC_1,    NONE,   CAN0_b,     NONE,      NONE,        R);
LAN966x_P(37,   GPIO, FC_SHRD0, PTPSYNC_2, TWI_SLC_GATE_AD, NONE, NONE,      NONE,        R);
LAN966x_P(38,   GPIO,    NONE,  PTPSYNC_3,    NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(39,   GPIO,    NONE,  PTPSYNC_4,    NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(40,   GPIO, FC_SHRD1, PTPSYNC_5,    NONE,     NONE,     NONE,      NONE,        R);
LAN966x_P(41,   GPIO, FC_SHRD2, PTPSYNC_6, TWI_SLC_GATE_AD, NONE, NONE,      NONE,        R);
LAN966x_P(42,   GPIO, FC_SHRD3, PTPSYNC_7, TWI_SLC_GATE_AD, NONE, NONE,      NONE,        R);
LAN966x_P(43,   GPIO,   FC2_b,   OB_TRG_b, IB_TRG_b, IRQ_OUT_a,  RECO_a,  IRQ_IN_a,        R);
LAN966x_P(44,   GPIO,   FC2_b,   OB_TRG_b, IB_TRG_b, IRQ_OUT_a,  RECO_a,  IRQ_IN_a,        R);
LAN966x_P(45,   GPIO,   FC2_b,   OB_TRG_b, IB_TRG_b, IRQ_OUT_a,    NONE,  IRQ_IN_a,        R);
LAN966x_P(46,   GPIO,   FC1_c,   OB_TRG_b, IB_TRG_b, IRQ_OUT_a, FC_SHRD4,  IRQ_IN_a,        R);
LAN966x_P(47,   GPIO,   FC1_c,   OB_TRG_b, IB_TRG_b, IRQ_OUT_a, FC_SHRD5,  IRQ_IN_a,        R);
LAN966x_P(48,   GPIO,   FC1_c,   OB_TRG_b, IB_TRG_b, IRQ_OUT_a, FC_SHRD6,  IRQ_IN_a,        R);
LAN966x_P(49,   GPIO, FC_SHRD7,  OB_TRG_b, IB_TRG_b, IRQ_OUT_a, TWI_SLC_GATE, IRQ_IN_a,    R);
LAN966x_P(50,   GPIO, FC_SHRD16, OB_TRG_b, IB_TRG_b, IRQ_OUT_a, TWI_SLC_GATE, NONE,        R);
LAN966x_P(51,   GPIO,   FC3_b,   OB_TRG_b, IB_TRG_c, IRQ_OUT_b,    NONE,  IRQ_IN_b,        R);
LAN966x_P(52,   GPIO,   FC3_b,   OB_TRG_b, IB_TRG_c, IRQ_OUT_b, TACHO_b,  IRQ_IN_b,        R);
LAN966x_P(53,   GPIO,   FC3_b,   OB_TRG_b, IB_TRG_c, IRQ_OUT_b,    NONE,  IRQ_IN_b,        R);
LAN966x_P(54,   GPIO, FC_SHRD8,  OB_TRG_b, IB_TRG_c, IRQ_OUT_b, TWI_SLC_GATE, IRQ_IN_b,    R);
LAN966x_P(55,   GPIO, FC_SHRD9,  OB_TRG_b, IB_TRG_c, IRQ_OUT_b, TWI_SLC_GATE, IRQ_IN_b,    R);
LAN966x_P(56,   GPIO,   FC4_b,   OB_TRG_b, IB_TRG_c, IRQ_OUT_b, FC_SHRD10,    IRQ_IN_b,    R);
LAN966x_P(57,   GPIO,   FC4_b, TWI_SLC_GATE, IB_TRG_c, IRQ_OUT_b, FC_SHRD11, IRQ_IN_b,    R);
LAN966x_P(58,   GPIO,   FC4_b, TWI_SLC_GATE, IB_TRG_c, IRQ_OUT_b, FC_SHRD12, IRQ_IN_b,    R);
LAN966x_P(59,   GPIO,   QSPI1,   MIIM_c,      NONE,     NONE,  MIIM_Sb,      NONE,        R);
LAN966x_P(60,   GPIO,   QSPI1,   MIIM_c,      NONE,     NONE,  MIIM_Sb,      NONE,        R);
LAN966x_P(61,   GPIO,   QSPI1,     NONE,   SGPIO_b,    FC0_c,  MIIM_Sb,      NONE,        R);
LAN966x_P(62,   GPIO,   QSPI1, FC_SHRD13,  SGPIO_b,    FC0_c, TWI_SLC_GATE,  SFP_SD,      R);
LAN966x_P(63,   GPIO,   QSPI1, FC_SHRD14,  SGPIO_b,    FC0_c, TWI_SLC_GATE,  SFP_SD,      R);
LAN966x_P(64,   GPIO,   QSPI1,    FC4_c,   SGPIO_b, FC_SHRD15, TWI_SLC_GATE, SFP_SD,      R);
LAN966x_P(65,   GPIO, USB_H_a,    FC4_c,      NONE, IRQ_OUT_c, TWI_SLC_GATE_AD, NONE,     R);
LAN966x_P(66,   GPIO, USB_H_a,    FC4_c,   USB_S_a, IRQ_OUT_c, IRQ_IN_c,     NONE,        R);
LAN966x_P(67,   GPIO, EMMC_SD,     NONE,     QSPI2,     NONE,     NONE,      NONE,        R);
LAN966x_P(68,   GPIO, EMMC_SD,     NONE,     QSPI2,     NONE,     NONE,      NONE,        R);
LAN966x_P(69,   GPIO, EMMC_SD,     NONE,     QSPI2,     NONE,     NONE,      NONE,        R);
LAN966x_P(70,   GPIO, EMMC_SD,     NONE,     QSPI2,     NONE,     NONE,      NONE,        R);
LAN966x_P(71,   GPIO, EMMC_SD,     NONE,     QSPI2,     NONE,     NONE,      NONE,        R);
LAN966x_P(72,   GPIO, EMMC_SD,     NONE,     QSPI2,     NONE,     NONE,      NONE,        R);
LAN966x_P(73,   GPIO,    EMMC,     NONE,      NONE,       SD,     NONE,      NONE,        R);
LAN966x_P(74,   GPIO,    EMMC,     NONE, FC_SHRD17,       SD, TWI_SLC_GATE,  NONE,        R);
LAN966x_P(75,   GPIO,    EMMC,     NONE, FC_SHRD18,       SD, TWI_SLC_GATE,  NONE,        R);
LAN966x_P(76,   GPIO,    EMMC,     NONE, FC_SHRD19,       SD, TWI_SLC_GATE,  NONE,        R);
LAN966x_P(77,   GPIO, EMMC_SD,     NONE, FC_SHRD20,     NONE, TWI_SLC_GATE,  NONE,        R);

#define LAN966x_PIN(n) {                                      \
	.number = n,                                           \
	.name = "GPIO_"#n,                                     \
	.drv_data = &lan966x_pin_##n                          \
}

static const struct pinctrl_pin_desc lan966x_pins[] = {
	LAN966x_PIN(0),
	LAN966x_PIN(1),
	LAN966x_PIN(2),
	LAN966x_PIN(3),
	LAN966x_PIN(4),
	LAN966x_PIN(5),
	LAN966x_PIN(6),
	LAN966x_PIN(7),
	LAN966x_PIN(8),
	LAN966x_PIN(9),
	LAN966x_PIN(10),
	LAN966x_PIN(11),
	LAN966x_PIN(12),
	LAN966x_PIN(13),
	LAN966x_PIN(14),
	LAN966x_PIN(15),
	LAN966x_PIN(16),
	LAN966x_PIN(17),
	LAN966x_PIN(18),
	LAN966x_PIN(19),
	LAN966x_PIN(20),
	LAN966x_PIN(21),
	LAN966x_PIN(22),
	LAN966x_PIN(23),
	LAN966x_PIN(24),
	LAN966x_PIN(25),
	LAN966x_PIN(26),
	LAN966x_PIN(27),
	LAN966x_PIN(28),
	LAN966x_PIN(29),
	LAN966x_PIN(30),
	LAN966x_PIN(31),
	LAN966x_PIN(32),
	LAN966x_PIN(33),
	LAN966x_PIN(34),
	LAN966x_PIN(35),
	LAN966x_PIN(36),
	LAN966x_PIN(37),
	LAN966x_PIN(38),
	LAN966x_PIN(39),
	LAN966x_PIN(40),
	LAN966x_PIN(41),
	LAN966x_PIN(42),
	LAN966x_PIN(43),
	LAN966x_PIN(44),
	LAN966x_PIN(45),
	LAN966x_PIN(46),
	LAN966x_PIN(47),
	LAN966x_PIN(48),
	LAN966x_PIN(49),
	LAN966x_PIN(50),
	LAN966x_PIN(51),
	LAN966x_PIN(52),
	LAN966x_PIN(53),
	LAN966x_PIN(54),
	LAN966x_PIN(55),
	LAN966x_PIN(56),
	LAN966x_PIN(57),
	LAN966x_PIN(58),
	LAN966x_PIN(59),
	LAN966x_PIN(60),
	LAN966x_PIN(61),
	LAN966x_PIN(62),
	LAN966x_PIN(63),
	LAN966x_PIN(64),
	LAN966x_PIN(65),
	LAN966x_PIN(66),
	LAN966x_PIN(67),
	LAN966x_PIN(68),
	LAN966x_PIN(69),
	LAN966x_PIN(70),
	LAN966x_PIN(71),
	LAN966x_PIN(72),
	LAN966x_PIN(73),
	LAN966x_PIN(74),
	LAN966x_PIN(75),
	LAN966x_PIN(76),
	LAN966x_PIN(77),
};


static int lan966x_get_functions_count(struct pinctrl_dev *pctldev)
{
	return ARRAY_SIZE(lan966x_function_names);
}

static const char *lan966x_get_function_name(struct pinctrl_dev *pctldev,
					    unsigned int function)
{
	return lan966x_function_names[function];
}

static int lan966x_get_function_groups(struct pinctrl_dev *pctldev,
				      unsigned int function,
				      const char *const **groups,
				      unsigned *const num_groups)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	*groups  = info->func[function].groups;
	*num_groups = info->func[function].ngroups;

	return 0;
}

static int lan966x_pin_function_idx(struct lan966x_pinctrl *info,
				   unsigned int pin, unsigned int function)
{
	struct lan966x_pin_caps *p = info->desc->pins[pin].drv_data;
	int i;

	for (i = 0; i < LAN966x_FUNC_PER_PIN; i++) {
		if (function == p->functions[i])
			return i;
	}

	return -1;
}

#define REG_ALT(msb, info, p) (LAN966x_GPIO_ALT0 * (info)->stride + 4 * ((msb) + ((info)->stride * ((p) / 32))))

static int lan966x_pinmux_set_mux(struct pinctrl_dev *pctldev,
				 unsigned int selector, unsigned int group)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	struct lan966x_pin_caps *pin = info->desc->pins[group].drv_data;
	unsigned int p = pin->pin % 32;
	int f;

	f = lan966x_pin_function_idx(info, group, selector);
	if (f < 0)
		return -EINVAL;

	/*
	 * f is encoded on two bits.
	 * bit 0 of f goes in BIT(pin) of ALT[0], bit 1 of f goes in BIT(pin) of
	 * ALT[1], bit 2 of f goes in BIT(pin) of ALT[2]
	 * This is racy because both registers can't be updated at the same time
	 * but it doesn't matter much for now.
	 * Note: ALT0/ALT1/ALT2 are organized specially for 78 gpio targets
	 */
	regmap_update_bits(info->map, REG_ALT(0, info, pin->pin),
			   BIT(p), f << p);
	regmap_update_bits(info->map, REG_ALT(1, info, pin->pin),
			   BIT(p), (f >> 1) << p);
	regmap_update_bits(info->map, REG_ALT(2, info, pin->pin),
			   BIT(p), (f >> 2) << p);

	return 0;
}

#define REG(r, info, p) ((r) * (info)->stride + (4 * ((p) / 32)))

static int lan966x_gpio_set_direction(struct pinctrl_dev *pctldev,
				     struct pinctrl_gpio_range *range,
				     unsigned int pin, bool input)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	unsigned int p = pin % 32;

	regmap_update_bits(info->map, REG(LAN966x_GPIO_OE, info, pin), BIT(p),
			   input ? 0 : BIT(p));

	return 0;
}

static int lan966x_gpio_request_enable(struct pinctrl_dev *pctldev,
				      struct pinctrl_gpio_range *range,
				      unsigned int offset)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	unsigned int p = offset % 32;

	regmap_update_bits(info->map, REG_ALT(0, info, offset),
			   BIT(p), 0);
	regmap_update_bits(info->map, REG_ALT(1, info, offset),
			   BIT(p), 0);
	regmap_update_bits(info->map, REG_ALT(2, info, offset),
			   BIT(p), 0);

	return 0;
}

static const struct pinmux_ops lan966x_pmx_ops = {
	.get_functions_count = lan966x_get_functions_count,
	.get_function_name = lan966x_get_function_name,
	.get_function_groups = lan966x_get_function_groups,
	.set_mux = lan966x_pinmux_set_mux,
	.gpio_set_direction = lan966x_gpio_set_direction,
	.gpio_request_enable = lan966x_gpio_request_enable,
};

static int lan966x_pctl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->desc->npins;
}

static const char *lan966x_pctl_get_group_name(struct pinctrl_dev *pctldev,
					      unsigned int group)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	return info->desc->pins[group].name;
}

static int lan966x_pctl_get_group_pins(struct pinctrl_dev *pctldev,
				      unsigned int group,
				      const unsigned int **pins,
				      unsigned int *num_pins)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);

	*pins = &info->desc->pins[group].number;
	*num_pins = 1;

	return 0;
}

static int lan966x_hw_get_value(struct lan966x_pinctrl *info,
			       unsigned int pin,
			       unsigned int reg,
			       int *val)
{
	int ret = -EOPNOTSUPP;

	if (info->pincfg) {
		u32 regcfg = readl(info->pincfg + (pin * sizeof(u32)));

		ret = 0;
		switch (reg) {
		case PINCONF_BIAS:
			*val = regcfg & BIAS_BITS;
			break;

		case PINCONF_SCHMITT:
			*val = regcfg & SCHMITT_BIT;
			break;

		case PINCONF_DRIVE_STRENGTH:
			*val = regcfg & DRIVE_BITS;
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
		}
	}
	return ret;
}

static int lan966x_hw_set_value(struct lan966x_pinctrl *info,
			       unsigned int pin,
			       unsigned int reg,
			       int val)
{
	int ret = -EOPNOTSUPP;

	if (info->pincfg) {
		void __iomem *regaddr = info->pincfg + (pin * sizeof(u32));

		ret = 0;
		switch (reg) {
		case PINCONF_BIAS:
			lan966x_clrsetbits(regaddr, BIAS_BITS, val);
			break;

		case PINCONF_SCHMITT:
			lan966x_clrsetbits(regaddr, SCHMITT_BIT, val);
			break;

		case PINCONF_DRIVE_STRENGTH:
			if (val <= 3)
				lan966x_clrsetbits(regaddr, DRIVE_BITS, val);
			else
				ret = -EINVAL;
			break;

		default:
			ret = -EOPNOTSUPP;
			break;
		}
	}
	return ret;
}

static int lan966x_pinconf_get(struct pinctrl_dev *pctldev,
			      unsigned int pin, unsigned long *config)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	u32 param = pinconf_to_config_param(*config);
	int val, err;

	switch (param) {
	case PIN_CONFIG_BIAS_DISABLE:
	case PIN_CONFIG_BIAS_PULL_UP:
	case PIN_CONFIG_BIAS_PULL_DOWN:
		err = lan966x_hw_get_value(info, pin, PINCONF_BIAS, &val);
		if (err)
			return err;
		if (param == PIN_CONFIG_BIAS_DISABLE)
			val = (val == 0 ? true : false);
		else if (param == PIN_CONFIG_BIAS_PULL_DOWN)
			val = (val & BIAS_PD_BIT ? true : false);
		else    /* PIN_CONFIG_BIAS_PULL_UP */
			val = (val & BIAS_PU_BIT ? true : false);
		break;

	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		err = lan966x_hw_get_value(info, pin, PINCONF_SCHMITT, &val);
		if (err)
			return err;

		val = (val & SCHMITT_BIT ? true : false);
		break;

	case PIN_CONFIG_DRIVE_STRENGTH:
		err = lan966x_hw_get_value(info, pin, PINCONF_DRIVE_STRENGTH,
					  &val);
		if (err)
			return err;
		break;

	case PIN_CONFIG_OUTPUT:
		err = regmap_read(info->map, REG(LAN966x_GPIO_OUT, info, pin),
				  &val);
		if (err)
			return err;
		val = !!(val & BIT(pin % 32));
		break;

	case PIN_CONFIG_INPUT_ENABLE:
	case PIN_CONFIG_OUTPUT_ENABLE:
		err = regmap_read(info->map, REG(LAN966x_GPIO_OE, info, pin),
				  &val);
		if (err)
			return err;
		val = val & BIT(pin % 32);
		if (param == PIN_CONFIG_OUTPUT_ENABLE)
			val = !!val;
		else
			val = !val;
		break;

	default:
		return -EOPNOTSUPP;
	}

	*config = pinconf_to_config_packed(param, val);

	return 0;
}

static int lan966x_pinconf_set(struct pinctrl_dev *pctldev, unsigned int pin,
			      unsigned long *configs, unsigned int num_configs)
{
	struct lan966x_pinctrl *info = pinctrl_dev_get_drvdata(pctldev);
	u32 param, arg, p;
	int cfg, err = 0;

	for (cfg = 0; cfg < num_configs; cfg++) {
		param = pinconf_to_config_param(configs[cfg]);
		arg = pinconf_to_config_argument(configs[cfg]);

		switch (param) {
		case PIN_CONFIG_BIAS_DISABLE:
		case PIN_CONFIG_BIAS_PULL_UP:
		case PIN_CONFIG_BIAS_PULL_DOWN:
			arg = (param == PIN_CONFIG_BIAS_DISABLE) ? 0 :
			(param == PIN_CONFIG_BIAS_PULL_UP) ? BIAS_PU_BIT :
			BIAS_PD_BIT;

			err = lan966x_hw_set_value(info, pin, PINCONF_BIAS, arg);
			if (err)
				goto err;

			break;

		case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
			arg = arg ? SCHMITT_BIT : 0;
			err = lan966x_hw_set_value(info, pin, PINCONF_SCHMITT,
						  arg);
			if (err)
				goto err;

			break;

		case PIN_CONFIG_DRIVE_STRENGTH:
			err = lan966x_hw_set_value(info, pin,
						  PINCONF_DRIVE_STRENGTH,
						  arg);
			if (err)
				goto err;

			break;

		case PIN_CONFIG_OUTPUT_ENABLE:
		case PIN_CONFIG_INPUT_ENABLE:
		case PIN_CONFIG_OUTPUT:
			p = pin % 32;
			if (arg)
				regmap_write(info->map,
					     REG(LAN966x_GPIO_OUT_SET, info,
						 pin),
					     BIT(p));
			else
				regmap_write(info->map,
					     REG(LAN966x_GPIO_OUT_CLR, info,
						 pin),
					     BIT(p));
			regmap_update_bits(info->map,
					   REG(LAN966x_GPIO_OE, info, pin),
					   BIT(p),
					   param == PIN_CONFIG_INPUT_ENABLE ?
					   0 : BIT(p));
			break;

		default:
			err = -EOPNOTSUPP;
		}
	}
err:
	return err;
}

static const struct pinconf_ops lan966x_confops = {
	.is_generic = true,
	.pin_config_get = lan966x_pinconf_get,
	.pin_config_set = lan966x_pinconf_set,
	.pin_config_config_dbg_show = pinconf_generic_dump_config,
};

static const struct pinctrl_ops lan966x_pctl_ops = {
	.get_groups_count = lan966x_pctl_get_groups_count,
	.get_group_name = lan966x_pctl_get_group_name,
	.get_group_pins = lan966x_pctl_get_group_pins,
#ifdef CONFIG_OF
	.dt_node_to_map = pinconf_generic_dt_node_to_map_pin,
	.dt_free_map = pinconf_generic_dt_free_map,
#endif
};

static struct pinctrl_desc lan966x_desc = {
	.name = "lan966x-pinctrl",
	.pins = lan966x_pins,
	.npins = ARRAY_SIZE(lan966x_pins),
	.pctlops = &lan966x_pctl_ops,
	.pmxops = &lan966x_pmx_ops,
	.confops = &lan966x_confops,
	.owner = THIS_MODULE,
};

static int lan966x_create_group_func_map(struct device *dev,
					struct lan966x_pinctrl *info)
{
	int f, npins, i;
	u8 *pins = kcalloc(info->desc->npins, sizeof(u8), GFP_KERNEL);

	if (!pins)
		return -ENOMEM;

	for (f = 0; f < FUNC_MAX; f++) {
		for (npins = 0, i = 0; i < info->desc->npins; i++) {
			if (lan966x_pin_function_idx(info, i, f) >= 0)
				pins[npins++] = i;
		}

		if (!npins)
			continue;

		info->func[f].ngroups = npins;
		info->func[f].groups = devm_kcalloc(dev, npins, sizeof(char *),
						    GFP_KERNEL);
		if (!info->func[f].groups) {
			kfree(pins);
			return -ENOMEM;
		}

		for (i = 0; i < npins; i++)
			info->func[f].groups[i] =
				info->desc->pins[pins[i]].name;
	}

	kfree(pins);

	return 0;
}

static int lan966x_pinctrl_register(struct platform_device *pdev,
				   struct lan966x_pinctrl *info)
{
	int ret;

	ret = lan966x_create_group_func_map(&pdev->dev, info);
	if (ret) {
		dev_err(&pdev->dev, "Unable to create group func map.\n");
		return ret;
	}

	info->pctl = devm_pinctrl_register(&pdev->dev, info->desc, info);
	if (IS_ERR(info->pctl)) {
		dev_err(&pdev->dev, "Failed to register pinctrl\n");
		return PTR_ERR(info->pctl);
	}

	return 0;
}

static int lan966x_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);
	unsigned int val;

	regmap_read(info->map, REG(LAN966x_GPIO_IN, info, offset), &val);

	return !!(val & BIT(offset % 32));
}

static void lan966x_gpio_set(struct gpio_chip *chip, unsigned int offset,
			    int value)
{
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);

	if (value)
		regmap_write(info->map, REG(LAN966x_GPIO_OUT_SET, info, offset),
			     BIT(offset % 32));
	else
		regmap_write(info->map, REG(LAN966x_GPIO_OUT_CLR, info, offset),
			     BIT(offset % 32));
}

static int lan966x_gpio_get_direction(struct gpio_chip *chip,
				     unsigned int offset)
{
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);
	unsigned int val;

	regmap_read(info->map, REG(LAN966x_GPIO_OE, info, offset), &val);

	if (val & BIT(offset % 32))
		return GPIO_LINE_DIRECTION_OUT;

	return GPIO_LINE_DIRECTION_IN;
}

static int lan966x_gpio_direction_input(struct gpio_chip *chip,
				       unsigned int offset)
{
	return pinctrl_gpio_direction_input(chip->base + offset);
}

static int lan966x_gpio_direction_output(struct gpio_chip *chip,
					unsigned int offset, int value)
{
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);
	unsigned int pin = BIT(offset % 32);

	if (value)
		regmap_write(info->map, REG(LAN966x_GPIO_OUT_SET, info, offset),
			     pin);
	else
		regmap_write(info->map, REG(LAN966x_GPIO_OUT_CLR, info, offset),
			     pin);

	return pinctrl_gpio_direction_output(chip->base + offset);
}

static const struct gpio_chip lan966x_gpiolib_chip = {
	.request = gpiochip_generic_request,
	.free = gpiochip_generic_free,
	.set = lan966x_gpio_set,
	.get = lan966x_gpio_get,
	.get_direction = lan966x_gpio_get_direction,
	.direction_input = lan966x_gpio_direction_input,
	.direction_output = lan966x_gpio_direction_output,
	.owner = THIS_MODULE,
};

static void lan966x_irq_mask(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);
	unsigned int gpio = irqd_to_hwirq(data);

	regmap_update_bits(info->map, REG(LAN966x_GPIO_INTR_ENA, info, gpio),
			   BIT(gpio % 32), 0);
}

static void lan966x_irq_unmask(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);
	unsigned int gpio = irqd_to_hwirq(data);

	regmap_update_bits(info->map, REG(LAN966x_GPIO_INTR_ENA, info, gpio),
			   BIT(gpio % 32), BIT(gpio % 32));
}

static void lan966x_irq_ack(struct irq_data *data)
{
	struct gpio_chip *chip = irq_data_get_irq_chip_data(data);
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);
	unsigned int gpio = irqd_to_hwirq(data);

	regmap_write_bits(info->map, REG(LAN966x_GPIO_INTR, info, gpio),
			  BIT(gpio % 32), BIT(gpio % 32));
}

static int lan966x_irq_set_type(struct irq_data *data, unsigned int type);

static struct irq_chip lan966x_eoi_irqchip = {
	.name		= "gpio",
	.irq_mask	= lan966x_irq_mask,
	.irq_eoi	= lan966x_irq_ack,
	.irq_unmask	= lan966x_irq_unmask,
	.flags          = IRQCHIP_EOI_THREADED | IRQCHIP_EOI_IF_HANDLED,
	.irq_set_type	= lan966x_irq_set_type,
};

static struct irq_chip lan966x_irqchip = {
	.name		= "gpio",
	.irq_mask	= lan966x_irq_mask,
	.irq_ack	= lan966x_irq_ack,
	.irq_unmask	= lan966x_irq_unmask,
	.irq_set_type	= lan966x_irq_set_type,
};

static int lan966x_irq_set_type(struct irq_data *data, unsigned int type)
{
	type &= IRQ_TYPE_SENSE_MASK;

	if (!(type & (IRQ_TYPE_EDGE_BOTH | IRQ_TYPE_LEVEL_HIGH)))
		return -EINVAL;

	if (type & IRQ_TYPE_LEVEL_HIGH)
		irq_set_chip_handler_name_locked(data, &lan966x_eoi_irqchip,
						 handle_fasteoi_irq, NULL);
	if (type & IRQ_TYPE_EDGE_BOTH)
		irq_set_chip_handler_name_locked(data, &lan966x_irqchip,
						 handle_edge_irq, NULL);

	return 0;
}

static void lan966x_irq_handler(struct irq_desc *desc)
{
	struct irq_chip *parent_chip = irq_desc_get_chip(desc);
	struct gpio_chip *chip = irq_desc_get_handler_data(desc);
	struct lan966x_pinctrl *info = gpiochip_get_data(chip);
	unsigned int id_reg = LAN966x_GPIO_INTR_IDENT * info->stride;
	unsigned int reg = 0, irq, i;
	unsigned long irqs;

	for (i = 0; i < info->stride; i++) {
		regmap_read(info->map, id_reg + 4 * i, &reg);
		if (!reg)
			continue;

		chained_irq_enter(parent_chip, desc);

		irqs = reg;

		for_each_set_bit(irq, &irqs,
				 min(32U, info->desc->npins - 32 * i))
			generic_handle_irq(irq_linear_revmap(chip->irq.domain,
							     irq + 32 * i));

		chained_irq_exit(parent_chip, desc);
	}
}

static int lan966x_gpiochip_register(struct platform_device *pdev,
				    struct lan966x_pinctrl *info)
{
	struct gpio_chip *gc;
	struct gpio_irq_chip *girq;
	int irq;

	info->gpio_chip = lan966x_gpiolib_chip;

	gc = &info->gpio_chip;
	gc->ngpio = info->desc->npins;
	gc->parent = &pdev->dev;
	gc->base = 0;
	gc->label = "lan966x-gpio";

#if defined(CONFIG_OF_GPIO)
	gc->of_node = info->dev->of_node;
#endif

	irq = platform_get_irq(pdev, 0);
	if (irq) {
		girq = &gc->irq;
		girq->chip = &lan966x_irqchip;
		girq->parent_handler = lan966x_irq_handler;
		girq->num_parents = 1;
		girq->parents = devm_kcalloc(&pdev->dev, 1,
					     sizeof(*girq->parents),
					     GFP_KERNEL);
		if (!girq->parents)
			return -ENOMEM;
		girq->parents[0] = irq;
		girq->default_type = IRQ_TYPE_NONE;
		girq->handler = handle_edge_irq;
	}

	return devm_gpiochip_add_data(&pdev->dev, gc, info);
}

static const struct of_device_id lan966x_pinctrl_of_match[] = {
	{ .compatible = "microchip,lan966x-pinctrl", .data = &lan966x_desc },
	{},
};

static int lan966x_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct lan966x_pinctrl *info;
	void __iomem *base;
	struct resource *res;
	int ret;
	struct regmap_config regmap_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
	};

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->desc = (struct pinctrl_desc *)device_get_match_data(dev);
	if (!info->desc)
		return -EINVAL;

	base = devm_ioremap_resource(dev,
			platform_get_resource(pdev, IORESOURCE_MEM, 0));
	if (IS_ERR(base)) {
		dev_err(dev, "Failed to ioremap registers\n");
		return PTR_ERR(base);
	}

	info->stride = 1 + (info->desc->npins - 1) / 32;

	regmap_config.max_register = LAN966x_GPIO_SD_MAP * info->stride + 15 * 4;

	info->map = devm_regmap_init_mmio(dev, base, &regmap_config);
	if (IS_ERR(info->map)) {
		dev_err(dev, "Failed to create regmap\n");
		return PTR_ERR(info->map);
	}
	dev_set_drvdata(dev, info->map);
	info->dev = dev;

	/* Pinconf registers */
	if (info->desc->confops) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		base = devm_ioremap_resource(dev, res);
		if (IS_ERR(base))
			dev_dbg(dev, "Failed to ioremap config registers (no extended pinconf)\n");
		else
			info->pincfg = base;
	}

	ret = lan966x_pinctrl_register(pdev, info);
	if (ret)
		return ret;

	ret = lan966x_gpiochip_register(pdev, info);
	if (ret)
		return ret;

	dev_info(dev, "driver registered\n");

	return 0;
}

static struct platform_driver lan966x_pinctrl_driver = {
	.driver = {
		.name = "pinctrl-lan966x",
		.of_match_table = lan966x_pinctrl_of_match,
		.suppress_bind_attrs = true,
	},
	.probe = lan966x_pinctrl_probe,
};
builtin_platform_driver(lan966x_pinctrl_driver);
