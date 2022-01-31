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
#include <linux/pinctrl/machine.h>
#include <linux/property.h>
#include <linux/pci_ids.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_device.h>
#include <dt-bindings/clock/microchip,lan966x.h>
#include <dt-bindings/mfd/atmel-flexcom.h>
#include <dt-bindings/soc/mchp,lan966x_icpu.h>
#include <dt-bindings/phy/phy-lan966x-serdes.h>
#include <dt-bindings/gpio/gpio.h>

#include <lan966x_pci_mfd.h>

#define PCI_DEVICE_ID_MCHP_LAN966X	0x9660

#define CPU_RESET_PROT_STAT_OFFSET	0x88
#define CPU_TARGET_OFFSET		0xc0000
#define CPU_TARGET_LENGTH		0x10000

#define LAN966X_BAR_CPU_OFFSET		0xe2000000
#define LAN966X_BAR_CPU_SIZE		SZ_32M
#define LAN966X_BAR_AMBA_OFFSET		0xe0000000
#define LAN966X_BAR_AMBA_SIZE		SZ_16M

#define LAN966X_DEV_ADDR(__res)	(__res ## _ADDR)
#define LAN966X_DEV_END(__res)	(LAN966X_DEV_ADDR(__res) + (__res ## _SIZE) - 1)
#define LAN966X_DEV_LEN(__res)	(__res ## _SIZE)

struct pci_dev_data {
	u8 bar_id;
};

static const resource_size_t bar_offsets[] = {
	[LAN966X_BAR_CPU] = LAN966X_BAR_CPU_OFFSET,
	[LAN966X_BAR_AMBA] = LAN966X_BAR_AMBA_OFFSET,
};

/* Fixed-clocks */
static const struct property_entry cpu_clk_props[] = {
	PROPERTY_ENTRY_U32("clock-frequency", 600000000),
	PROPERTY_ENTRY_U32("#clock-cells", 0),
	{}
};

static const struct software_node cpu_clk_node = {
	.name = "cpu_clk",
	.properties = cpu_clk_props,
};

static const struct property_entry ddr_clk_props[] = {
	PROPERTY_ENTRY_U32("clock-frequency", 30000000),
	PROPERTY_ENTRY_U32("#clock-cells", 0),
	{}
};

static const struct software_node ddr_clk_node = {
	.name = "ddr_clk",
	.properties = ddr_clk_props,
};

static const struct property_entry sys_clk_props[] = {
	PROPERTY_ENTRY_U32("clock-frequency", 15625000),
	PROPERTY_ENTRY_U32("#clock-cells", 0),
	{}
};

static const struct software_node sys_clk_node = {
	.name = "sys_clk",
	.properties = sys_clk_props,
};

/* Lan966X Clocks */
static const struct property_entry lan966x_clk_props[] = {
	PROPERTY_ENTRY_U32("#clock-cells", 1),
	{}
};

static const struct software_node lan966x_clk_node = {
	.name = "lan966x_clk",
	.properties = lan966x_clk_props,
};

/* PINCTRL */
static const struct software_node lan966x_pinctrl_node;

/* I2C SDA/SCL configuration */
static const char * const i2c_pins[] = {
	"GPIO_9",
	"GPIO_10",
};

static const struct property_entry lan966x_i2c_pinctrl_props[] = {
	PROPERTY_ENTRY_STRING_ARRAY("pins", i2c_pins),
	PROPERTY_ENTRY_STRING("function", "fc0_a"),
	{}
};

static const struct software_node lan966x_i2c_pinmux_node = {
	.name = "i2c-pinctrl-0",
	.properties = lan966x_i2c_pinctrl_props,
	.parent = &lan966x_pinctrl_node,
};

/* I2C idle pinmux configuration */
static const char * const i2c_mux_pins[] = {
	"GPIO_76",
	"GPIO_77",
};

static const struct property_entry lan966x_i2c_idle_pinmux_props[] = {
	PROPERTY_ENTRY_STRING_ARRAY("pins", i2c_mux_pins),
	PROPERTY_ENTRY_STRING("function", "twi_slc_gate"),
	PROPERTY_ENTRY_BOOL("output-low"),
	{}
};

static const struct software_node lan966x_i2c_idle_pinmux_node = {
	.name = "i2c-idle-pinmux",
	.properties = lan966x_i2c_idle_pinmux_props,
	.parent = &lan966x_pinctrl_node,
};

/* I2C i2c102 pinmux configuration */
static const struct property_entry lan966x_i2c_i2c102_pinmux_props[] = {
	PROPERTY_ENTRY_STRING("pins", "GPIO_76"),
	PROPERTY_ENTRY_STRING("function", "twi_slc_gate"),
	PROPERTY_ENTRY_BOOL("output-high"),
	{}
};

static const struct software_node lan966x_i2c_i2c102_pinmux_node = {
	.name = "i2c-i2c102-pinmux",
	.properties = lan966x_i2c_i2c102_pinmux_props,
	.parent = &lan966x_pinctrl_node,
};

/* I2C i2c103 pinmux configuration */
static const struct property_entry lan966x_i2c_i2c103_pinmux_props[] = {
	PROPERTY_ENTRY_STRING("pins", "GPIO_77"),
	PROPERTY_ENTRY_STRING("function", "twi_slc_gate"),
	PROPERTY_ENTRY_BOOL("output-high"),
	{}
};

static const struct software_node lan966x_i2c_i2c103_pinmux_node = {
	.name = "i2c-i2c103-pinmux",
	.properties = lan966x_i2c_i2c103_pinmux_props,
	.parent = &lan966x_pinctrl_node,
};

/* Switch PTP pin */
static const struct property_entry lan966x_switch_ptp_pinmux_props[] = {
	PROPERTY_ENTRY_STRING("pins", "GPIO_36"),
	PROPERTY_ENTRY_STRING("function", "ptpsync_1"),
	{}
};

static const struct software_node lan966x_switch_ptp_pinmux_node = {
	.name = "ptp-pinmux",
	.properties = lan966x_switch_ptp_pinmux_props,
	.parent = &lan966x_pinctrl_node,
};

static const struct property_entry lan966x_pinctrl_props[] = {
	PROPERTY_ENTRY_STRING("compatible", "microchip,lan966x-pinctrl"),
	PROPERTY_ENTRY_U32("#gpio-cells", 2),
	PROPERTY_ENTRY_REF("gpio-ranges", &lan966x_pinctrl_node, 0, 0, 78),
	{}
};

static const struct software_node lan966x_pinctrl_node = {
	.name = "pinctrl-lan966x",
	.properties = lan966x_pinctrl_props,
};

/* FLEXCOM */
static const struct property_entry lan966x_flexcom_props[] = {
	PROPERTY_ENTRY_U32("atmel,flexcom-mode", ATMEL_FLEXCOM_MODE_TWI),
	PROPERTY_ENTRY_REF("clocks", &ddr_clk_node),
	{}
};

static const struct software_node lan966x_flexcom_node = {
	.name = "lan966x-flexcom",
	.properties = lan966x_flexcom_props,
};

/* I2C */
static const struct property_entry lan966x_i2c_props[] = {
	PROPERTY_ENTRY_BOOL("i2c-analog-filter"),
	PROPERTY_ENTRY_BOOL("i2c-digital-filter"),
	PROPERTY_ENTRY_U32("i2c-digital-filter-width-ns", 35),
	PROPERTY_ENTRY_REF("clocks", &lan966x_clk_node, GCK_ID_FLEXCOM0),
	PROPERTY_ENTRY_STRING("pinctrl-names", "default"),
	PROPERTY_ENTRY_REF("pinctrl-0", &lan966x_i2c_pinmux_node),
	{ }
};

static const struct software_node lan966x_i2c_node = {
	.name = "lan966x-i2c",
	.properties = lan966x_i2c_props,
};

/* I2C mux pinctrl */
static const char *pinctrl_names[] = { "i2c102", "i2c103", "idle" };

static const struct property_entry lan966x_i2c_mux_pinctrl_props[] = {
	PROPERTY_ENTRY_STRING_ARRAY("pinctrl-names", pinctrl_names),
	PROPERTY_ENTRY_REF("i2c-parent", &lan966x_i2c_node),
	PROPERTY_ENTRY_REF("pinctrl-0", &lan966x_i2c_i2c102_pinmux_node),
	PROPERTY_ENTRY_REF("pinctrl-1", &lan966x_i2c_i2c103_pinmux_node),
	PROPERTY_ENTRY_REF("pinctrl-2", &lan966x_i2c_idle_pinmux_node),
	{ }
};

static const struct software_node lan966x_i2c_mux_pinctrl_node = {
	.name = "i2c-mux-pinctrl",
	.properties = lan966x_i2c_mux_pinctrl_props,
};

/* I2C mux pinctrl mux 0 */
static const struct property_entry lan966x_i2c_mux_pinctrl_0_props[] = {
	PROPERTY_ENTRY_U32("reg", 0),
	{}
};

static const struct software_node lan966x_i2c_mux_pinctrl_0_node = {
	.name = "i2c-mux-pinctrl-0",
	.properties = lan966x_i2c_mux_pinctrl_0_props,
	.parent = &lan966x_i2c_mux_pinctrl_node,
};

/* I2C mux pinctrl mux 1 */
static const struct property_entry lan966x_i2c_mux_pinctrl_1_props[] = {
	PROPERTY_ENTRY_U32("reg", 1),
	{}
};

static const struct software_node lan966x_i2c_mux_pinctrl_1_node = {
	.name = "i2c-mux-pinctrl-1",
	.properties = lan966x_i2c_mux_pinctrl_1_props,
	.parent = &lan966x_i2c_mux_pinctrl_node,
};

/* SFP 1 */
static const struct property_entry lan966x_sfp0_props[] = {
	PROPERTY_ENTRY_STRING("compatible", "sff,sfp"),
	PROPERTY_ENTRY_REF("i2c-bus", &lan966x_i2c_mux_pinctrl_0_node),
	PROPERTY_ENTRY_REF("tx-disable-gpios", &lan966x_pinctrl_node, 0, GPIO_ACTIVE_HIGH),
	PROPERTY_ENTRY_REF("los-gpios", &lan966x_pinctrl_node, 25, GPIO_ACTIVE_HIGH),
	PROPERTY_ENTRY_REF("mod-def0-gpios", &lan966x_pinctrl_node, 18, GPIO_ACTIVE_LOW),
	PROPERTY_ENTRY_REF("tx-fault-gpios", &lan966x_pinctrl_node, 2, GPIO_ACTIVE_HIGH),
	{ }
};

static const struct software_node lan966x_sfp0_node = {
	.name = "sfp0",
	.properties = lan966x_sfp0_props,
};

/* SFP2 */
static const struct property_entry lan966x_sfp1_props[] = {
	PROPERTY_ENTRY_STRING("compatible", "sff,sfp"),
	PROPERTY_ENTRY_REF("i2c-bus", &lan966x_i2c_mux_pinctrl_1_node),
	PROPERTY_ENTRY_REF("tx-disable-gpios", &lan966x_pinctrl_node, 1, GPIO_ACTIVE_HIGH),
	PROPERTY_ENTRY_REF("los-gpios", &lan966x_pinctrl_node, 26, GPIO_ACTIVE_HIGH),
	PROPERTY_ENTRY_REF("mod-def0-gpios", &lan966x_pinctrl_node, 19, GPIO_ACTIVE_LOW),
	PROPERTY_ENTRY_REF("tx-fault-gpios", &lan966x_pinctrl_node, 3, GPIO_ACTIVE_HIGH),
	{ }
};

static const struct software_node lan966x_sfp1_node = {
	.name = "sfp1",
	.properties = lan966x_sfp1_props,
};

static const struct software_node lan966x_cpu_ctrl_node = {
	.name = "cpu-ctrl",
};

/* Reset */
static const struct property_entry lan966x_phy_reset_props[] = {
	PROPERTY_ENTRY_U32("#reset-cells", 1),
};

static const struct software_node lan966x_phy_reset_node = {
	.name = "phy-reset",
	.properties = lan966x_phy_reset_props,
};

static const struct property_entry lan966x_switch_reset_props[] = {
	PROPERTY_ENTRY_STRING("compatible", "microchip,lan966x-switch-reset"),
	PROPERTY_ENTRY_U32("#reset-cells", 1),
	PROPERTY_ENTRY_REF("cpu-syscon", &lan966x_cpu_ctrl_node),
};

static const struct software_node lan966x_switch_reset_node = {
	.name = "reset",
	.properties = lan966x_switch_reset_props,
};

/* MDIO */
static const struct property_entry lan966x_mdio1_props[] = {
	PROPERTY_ENTRY_STRING("compatible", "microchip,lan966x-miim"),
	PROPERTY_ENTRY_REF("resets", &lan966x_phy_reset_node, 0),
	PROPERTY_ENTRY_STRING("reset-names", "phy"),
};

static const struct software_node lan966x_mdio1_node = {
	.name = "mdio1",
	.properties = lan966x_mdio1_props,
};

static const struct property_entry lan966x_phy_0_props[] = {
	PROPERTY_ENTRY_U32("reg", 1),
	{}
};

static const struct software_node lan966x_phy_0_node = {
	.name = "phy0",
	.properties = lan966x_phy_0_props,
	.parent = &lan966x_mdio1_node,
};

static const struct property_entry lan966x_phy_1_props[] = {
	PROPERTY_ENTRY_U32("reg", 2),
	{}
};

static const struct software_node lan966x_phy_1_node = {
	.name = "phy1",
	.properties = lan966x_phy_1_props,
	.parent = &lan966x_mdio1_node,
};

/* Serdes */
static const struct property_entry lan966x_serdes_props[] = {
	PROPERTY_ENTRY_STRING("compatible", "microchip,lan966x-serdes"),
	PROPERTY_ENTRY_U32("#phy-cells", 2),
};

static const struct software_node lan966x_serdes_node = {
	.name = "serdes",
	.properties = lan966x_serdes_props,
};

/* Switch */

static struct software_node_ref_args switch_resets[] = {
	{ .node = &lan966x_switch_reset_node, .nargs = 1, .args = {0}},
	{ .node = &lan966x_phy_reset_node, .nargs = 1, .args = {0}},
};

static const char *const switch_reset_names[] = {"switch", "phy"};

static const struct property_entry lan966x_switch_props[] = {
	PROPERTY_ENTRY_STRING("compatible", "microchip,lan966x-switch"),
	PROPERTY_ENTRY_STRING_ARRAY("reset-names", switch_reset_names),
	PROPERTY_ENTRY_REF_ARRAY("resets", switch_resets),
	PROPERTY_ENTRY_STRING("pinctrl-names", "default"),
	PROPERTY_ENTRY_REF("pinctrl-0", &lan966x_switch_ptp_pinmux_node),
};

static const struct software_node lan966x_switch_node = {
	.name = "switch",
	.properties = lan966x_switch_props,
};

static const struct software_node lan966x_switch_ports_node = {
	.name = "ethernet-ports",
	.parent = &lan966x_switch_node,
};

/* MDIO ports */
static const struct property_entry lan966x_switch_port0_props[] = {
	PROPERTY_ENTRY_U32("reg", 0),
	PROPERTY_ENTRY_REF("phy-handle", &lan966x_phy_0_node),
	PROPERTY_ENTRY_STRING("phy-mode", "gmii"),
	PROPERTY_ENTRY_REF("phys", &lan966x_serdes_node, 0, CU(0)),
};

static const struct software_node lan966x_switch_port0_node = {
	.name = "port0",
	.properties = lan966x_switch_port0_props,
	.parent = &lan966x_switch_ports_node,
};

static const struct property_entry lan966x_switch_port1_props[] = {
	PROPERTY_ENTRY_U32("reg", 1),
	PROPERTY_ENTRY_REF("phy-handle", &lan966x_phy_1_node),
	PROPERTY_ENTRY_STRING("phy-mode", "gmii"),
	PROPERTY_ENTRY_REF("phys", &lan966x_serdes_node, 1, CU(1)),
};

static const struct software_node lan966x_switch_port1_node = {
	.name = "port1",
	.properties = lan966x_switch_port1_props,
	.parent = &lan966x_switch_ports_node,
};

/* SFP ports */
static const struct property_entry lan966x_switch_port2_props[] = {
	PROPERTY_ENTRY_U32("reg", 2),
	PROPERTY_ENTRY_STRING("phy-mode", "sgmii"),
	PROPERTY_ENTRY_STRING("managed", "in-band-status"),
	PROPERTY_ENTRY_REF("phys", &lan966x_serdes_node, 2, SERDES6G(0)),
	PROPERTY_ENTRY_REF("sfp", &lan966x_sfp0_node),
};

static const struct software_node lan966x_switch_port2_node = {
	.name = "port2",
	.properties = lan966x_switch_port2_props,
	.parent = &lan966x_switch_ports_node,
};

static const struct property_entry lan966x_switch_port3_props[] = {
	PROPERTY_ENTRY_U32("reg", 3),
	PROPERTY_ENTRY_STRING("phy-mode", "sgmii"),
	PROPERTY_ENTRY_STRING("managed", "in-band-status"),
	PROPERTY_ENTRY_REF("phys", &lan966x_serdes_node, 3, SERDES6G(1)),
	PROPERTY_ENTRY_REF("sfp", &lan966x_sfp1_node),
};

static const struct software_node lan966x_switch_port3_node = {
	.name = "port3",
	.properties = lan966x_switch_port3_props,
	.parent = &lan966x_switch_ports_node,
};

/* PCI device */
static struct pci_device_id lan966x_ids[] = {
	{ PCI_DEVICE(PCI_VENDOR_ID_EFAR, PCI_DEVICE_ID_MCHP_LAN966X) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, lan966x_ids);

enum lan966x_dev {
	LAN966X_DEV_CPU_CLK = 0,
	LAN966X_DEV_DDR_CLK,
	LAN966X_DEV_SYS_CLK,
	LAN966X_DEV_CLK,
	LAN966X_DEV_PINCTRL,
	LAN966X_DEV_FLEXCOM,
	LAN966X_DEV_I2C,
	LAN966X_DEV_I2C_MUX_PINCTRL,
	LAN966X_DEV_SFP0,
	LAN966X_DEV_SFP1,
	LAN966X_DEV_CPU_CTRL,
	LAN966X_DEV_SWITCH_RESET,
	LAN966X_DEV_PHY_RESET,
	LAN966X_DEV_MDIO1,
	LAN966X_DEV_SERDES,
	LAN966X_DEV_SWITCH,
};

static struct resource lan966x_clk_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(CPU_GCK_REGS),
		.end = LAN966X_DEV_END(CPU_GCK_REGS),
	},
};

static struct resource lan966x_pinctrl_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(GCB_GPIO),
		.end = LAN966X_DEV_END(GCB_GPIO),
	},
	[1] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(CHIP_TOP_GPIO_CFG),
		.end = LAN966X_DEV_END(CHIP_TOP_GPIO_CFG),
	},
	[2] = {
		.flags = IORESOURCE_IRQ,
		.start = 17,
		.end = 17,
	},
};

static struct resource lan966x_flexcom_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(FLEXCOM_0_FLEXCOM_REG),
		.end = LAN966X_DEV_ADDR(FLEXCOM_0_FLEXCOM_REG),
	},
};

static struct resource lan966x_i2c_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(FLEXCOM_0_FLEXCOM_TWI_REG),
		.end = LAN966X_DEV_END(FLEXCOM_0_FLEXCOM_TWI_REG),
	},
	[1] = {
		.flags = IORESOURCE_IRQ,
		.start = 48,
		.end = 48,
	},
};

/* Reset */
static struct resource lan966x_cpu_ctrl_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(CPU),
		.end = LAN966X_DEV_END(CPU),
	},
};

static struct resource lan966x_switch_reset_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(GCB_CHIP_REGS_SOFT_RST),
		.end = LAN966X_DEV_END(GCB_CHIP_REGS_SOFT_RST),
	},
};

static struct resource lan966x_phy_reset_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(CHIP_TOP_CUPHY_CFG),
		.end = LAN966X_DEV_END(CHIP_TOP_CUPHY_CFG),
		.name = "phy"
	},
};

/* MDIO */
static struct resource lan966x_mdio1_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(GCB_MIIM_1),
		.end = LAN966X_DEV_END(GCB_MIIM_1),
	},
};

/* HSIO */
static struct resource lan966x_serdes_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_DEV_ADDR(HSIO),
		.end = LAN966X_DEV_END(HSIO),
	},
	[1] = {
		.flags = IORESOURCE_MEM,
		.start = 0xe2004010,
		.end = 0xe2004014,
	},
};

/* Switch */
static struct resource lan966x_switch_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_BAR_AMBA_OFFSET,
		.end = LAN966X_BAR_AMBA_OFFSET + LAN966X_BAR_AMBA_SIZE - 1,
		.name = "cpu",
	},
	[1] = {
		.flags = IORESOURCE_MEM,
		.start = LAN966X_BAR_CPU_OFFSET,
		.end = LAN966X_BAR_CPU_OFFSET + LAN966X_BAR_CPU_SIZE - 1,
		.name = "gcb",
	},
	[2] = {
		.flags = IORESOURCE_IRQ,
		.start = 12,
		.end = 12,
		.name = "xtr",
	},
	[3] = {
		.flags = IORESOURCE_IRQ,
		.start = 9,
		.end = 9,
		.name = "ana",
	},
};

static struct mfd_cell lan966x_pci_mfd_cells[] = {
	[LAN966X_DEV_CPU_CLK] = {
		.name = "of_fixed_clk",
		.swnode = &cpu_clk_node,
	},
	[LAN966X_DEV_DDR_CLK] = {
		.name = "of_fixed_clk",
		.swnode = &ddr_clk_node,
	},
	[LAN966X_DEV_SYS_CLK] = {
		.name = "of_fixed_clk",
		.swnode = &sys_clk_node,
	},
	[LAN966X_DEV_CLK] = {
		.name = "lan966x-clk",
		.num_resources = ARRAY_SIZE(lan966x_clk_res),
		.resources = lan966x_clk_res,
		.swnode = &lan966x_clk_node,
	},
	[LAN966X_DEV_PINCTRL] = {
		.name = "pinctrl-lan966x",
		.num_resources = ARRAY_SIZE(lan966x_pinctrl_res),
		.resources = lan966x_pinctrl_res,
		.swnode = &lan966x_pinctrl_node,
	},
	[LAN966X_DEV_FLEXCOM] = {
		.name = "atmel_flexcom",
		.num_resources = ARRAY_SIZE(lan966x_flexcom_res),
		.resources = lan966x_flexcom_res,
		.swnode = &lan966x_flexcom_node,
	},
	[LAN966X_DEV_I2C] = {
		.name = "lan966x-i2c",
		.num_resources = ARRAY_SIZE(lan966x_i2c_res),
		.resources = lan966x_i2c_res,
		.swnode = &lan966x_i2c_node,
	},
	[LAN966X_DEV_I2C_MUX_PINCTRL] = {
		.name = "i2c-mux-pinctrl",
		.swnode = &lan966x_i2c_mux_pinctrl_node,
	},
	[LAN966X_DEV_SFP0] = {
		.name = "sfp",
		.swnode = &lan966x_sfp0_node,
	},
	[LAN966X_DEV_SFP1] = {
		.name = "sfp",
		.swnode = &lan966x_sfp1_node,
	},
	[LAN966X_DEV_CPU_CTRL] = {
		.name = "syscon",
		.num_resources = ARRAY_SIZE(lan966x_cpu_ctrl_res),
		.resources = lan966x_cpu_ctrl_res,
		.swnode = &lan966x_cpu_ctrl_node,
	},
	[LAN966X_DEV_SWITCH_RESET] = {
		.num_resources = ARRAY_SIZE(lan966x_switch_reset_res),
		.resources = lan966x_switch_reset_res,
		.name = "sparx5-switch-reset",
		.swnode = &lan966x_switch_reset_node,
	},
	[LAN966X_DEV_PHY_RESET] = {
		.name = "lan966x-phy-reset",
		.num_resources = ARRAY_SIZE(lan966x_phy_reset_res),
		.resources = lan966x_phy_reset_res,
		.swnode = &lan966x_phy_reset_node,
	},
	[LAN966X_DEV_MDIO1] = {
		.name = "mscc-miim",
		.num_resources = ARRAY_SIZE(lan966x_mdio1_res),
		.resources = lan966x_mdio1_res,
		.swnode = &lan966x_mdio1_node,
	},
	[LAN966X_DEV_SERDES] = {
		.name = "microchip,lan966x-serdes",
		.num_resources = ARRAY_SIZE(lan966x_serdes_res),
		.resources = lan966x_serdes_res,
		.swnode = &lan966x_serdes_node,
	},
	[LAN966X_DEV_SWITCH] = {
		.name = "lan966x-switch",
		.num_resources = ARRAY_SIZE(lan966x_switch_res),
		.resources = lan966x_switch_res,
		.swnode = &lan966x_switch_node,
	},
};

static const struct software_node *lan966x_nodes[] = {
	&cpu_clk_node,
	&ddr_clk_node,
	&sys_clk_node,
	&lan966x_clk_node,
	&lan966x_pinctrl_node,
	&lan966x_i2c_pinmux_node,
	&lan966x_i2c_idle_pinmux_node,
	&lan966x_i2c_i2c102_pinmux_node,
	&lan966x_i2c_i2c103_pinmux_node,
	&lan966x_switch_ptp_pinmux_node,
	&lan966x_flexcom_node,
	&lan966x_i2c_node,
	&lan966x_i2c_mux_pinctrl_node,
	&lan966x_i2c_mux_pinctrl_0_node,
	&lan966x_i2c_mux_pinctrl_1_node,
	&lan966x_sfp0_node,
	&lan966x_sfp1_node,
	&lan966x_cpu_ctrl_node,
	&lan966x_phy_reset_node,
	&lan966x_switch_reset_node,
	&lan966x_mdio1_node,
	&lan966x_phy_0_node,
	&lan966x_phy_1_node,
	&lan966x_serdes_node,
	&lan966x_switch_node,
	&lan966x_switch_ports_node,
	&lan966x_switch_port0_node,
	&lan966x_switch_port1_node,
	&lan966x_switch_port2_node,
	&lan966x_switch_port3_node,
	NULL,
};

static int lan966x_get_bar(resource_size_t addr)
{
	int bar_id = LAN966X_BAR_AMBA;

	if (addr >= LAN966X_BAR_CPU_OFFSET)
		bar_id = LAN966X_BAR_CPU;

	return bar_id;
}

static int lan966x_pci_setup_resource(struct lan966x_pci *data,
				      struct pci_dev *pdev, u8 id)
{
	int i, bar_id;
	struct mfd_cell *cell = &lan966x_pci_mfd_cells[id];
	resource_size_t pci_addr, bar_offset;

	for (i = 0; i < cell->num_resources; i++) {
		struct resource *r = (struct resource *) &cell->resources[i];

		if (r->flags == IORESOURCE_MEM) {
			bar_id = lan966x_get_bar(r->start);
			bar_offset = bar_offsets[bar_id];

			pci_addr = pci_resource_start(pdev, bar_id);
			r->start = (r->start - bar_offset) + pci_addr;
			r->end = (r->end - bar_offset) + pci_addr;

			if (r->end > pci_resource_end(pdev, bar_id)) {
				dev_err(&pdev->dev, "Resource too large for bar %d\n", bar_id);
				return -EINVAL;
			}
		} else if (r->flags == IORESOURCE_IRQ) {
			r->start = irq_find_mapping(data->irq_data.domain, r->start);
			r->end = irq_find_mapping(data->irq_data.domain, r->end);
			if (!r->start || !r->end) {
				dev_err(&pdev->dev, "Invalid irq number\n");
				return -EINVAL;
			}
		} else {
			dev_err(&pdev->dev, "Unknown resource flag\n");
			return -EINVAL;
		}
		dev_info(&pdev->dev, "Setting %s resource %d to %pr\n",
			 cell->name, i, r);
	}
	return 0;
}

static int lan966x_pci_setup_resources(struct lan966x_pci *data,
				       struct pci_dev *pdev)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(lan966x_pci_mfd_cells); i++) {
		if (lan966x_pci_mfd_cells[i].num_resources) {
			ret = lan966x_pci_setup_resource(data, pdev, i);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static int reset_switch(struct pci_dev *pdev)
{
	u32 rst_status;
	int ret = -EIO;
	void __iomem *gcb_regs, *cpu_regs;
	u64 offset = LAN966X_DEV_ADDR(GCB_CHIP_REGS_SOFT_RST) -
		     LAN966X_BAR_CPU_OFFSET;
	u32 len = LAN966X_DEV_LEN(GCB_CHIP_REGS_SOFT_RST);

	gcb_regs = pci_iomap_range(pdev, LAN966X_BAR_CPU, offset, len);
	if (!gcb_regs)
		return -EIO;

	cpu_regs = pci_iomap_range(pdev, LAN966X_BAR_AMBA, CPU_TARGET_OFFSET,
				   CPU_TARGET_LENGTH);
	if (!cpu_regs)
		goto err_unmap_gcb;

	/* Protect VCore from reset */
	writel(BIT(5), cpu_regs + CPU_RESET_PROT_STAT_OFFSET);

	/* Reset switch */
	writel(BIT(1), gcb_regs);

	ret = readl_poll_timeout(gcb_regs, rst_status,
				 (rst_status & BIT(1)) == 0, 1, 100);
	if (ret)
		pr_err("Failed to reset VCore, status: %x\n", rst_status);

	pci_iounmap(pdev, cpu_regs);
err_unmap_gcb:
	pci_iounmap(pdev, gcb_regs);

	return ret;
}

static int lan966x_pci_probe(struct pci_dev *pdev,
			      const struct pci_device_id *id)
{
	struct device *dev = &pdev->dev;
	struct lan966x_pci *data;
	int ret;

	ret = pcim_enable_device(pdev);
	if (ret)
		return ret;

	reset_switch(pdev);

	pci_set_master(pdev);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	dev_set_drvdata(dev, data);
	data->dev = dev;

	ret = lan966x_pci_irq_setup(pdev, &data->irq_data);
	if (ret)
		return ret;

	ret = lan966x_pci_setup_resources(data, pdev);
	if (ret)
		goto err_free_irq;

	ret = software_node_register_node_group(lan966x_nodes);
	if (ret)
		goto err_free_irq;

	ret = devm_mfd_add_devices(dev, PLATFORM_DEVID_AUTO,
				   lan966x_pci_mfd_cells,
				   ARRAY_SIZE(lan966x_pci_mfd_cells), NULL, 0,
				   NULL);
	if (ret)
		goto err_unregister_nodes;

	return ret;

err_unregister_nodes:
	software_node_unregister_node_group(lan966x_nodes);
err_free_irq:
	lan966x_pci_irq_remove(pdev, &data->irq_data);

	return ret;
}

static void lan966x_pci_remove(struct pci_dev *pdev)
{

	struct device *dev = &pdev->dev;
	struct lan966x_pci *data = dev_get_drvdata(dev);

	lan966x_pci_irq_remove(pdev, &data->irq_data);
	software_node_unregister_node_group(lan966x_nodes);
}

static struct pci_driver lan966x_pci_driver = {
	.name = "mchp_lan966x",
	.id_table = lan966x_ids,
	.probe = lan966x_pci_probe,
	.remove = lan966x_pci_remove,
};

module_pci_driver(lan966x_pci_driver);

MODULE_DESCRIPTION("Maserati PCI driver");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("Clément Léger <clement.leger@bootlin.com>");
