// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022, Advanced Micro Devices, Inc.
 */

#include <linux/pci.h>
#include <linux/of.h>
#include <linux/bitfield.h>
#include <linux/bits.h>
#include <asm/unaligned.h>
#include "pci.h"

#define OF_PCI_ADDRESS_CELLS		3
#define OF_PCI_SIZE_CELLS		2

struct of_pci_addr_pair {
	u32		phys_addr[OF_PCI_ADDRESS_CELLS];
	u32		size[OF_PCI_SIZE_CELLS];
};

struct of_pci_range {
	u32		child_addr[OF_PCI_ADDRESS_CELLS];
	u32		parent_addr[OF_PCI_ADDRESS_CELLS];
	u32		size[OF_PCI_SIZE_CELLS];
};

#define OF_PCI_ADDR_SPACE_CONFIG	0x0
#define OF_PCI_ADDR_SPACE_IO		0x1
#define OF_PCI_ADDR_SPACE_MEM32		0x2
#define OF_PCI_ADDR_SPACE_MEM64		0x3

#define OF_PCI_ADDR_FIELD_NONRELOC	BIT(31)
#define OF_PCI_ADDR_FIELD_SS		GENMASK(25, 24)
#define OF_PCI_ADDR_FIELD_PREFETCH	BIT(30)
#define OF_PCI_ADDR_FIELD_BUS		GENMASK(23, 16)
#define OF_PCI_ADDR_FIELD_DEV		GENMASK(15, 11)
#define OF_PCI_ADDR_FIELD_FUNC		GENMASK(10, 8)
#define OF_PCI_ADDR_FIELD_REG		GENMASK(7, 0)

#define OF_PCI_ADDR_HI			GENMASK_ULL(63, 32)
#define OF_PCI_ADDR_LO			GENMASK_ULL(31, 0)
#define OF_PCI_SIZE_HI			GENMASK_ULL(63, 32)
#define OF_PCI_SIZE_LO			GENMASK_ULL(31, 0)

enum of_pci_prop_compatible {
	PROP_COMPAT_PCI_VVVV_DDDD,
	PROP_COMPAT_PCICLASS_CCSSPP,
	PROP_COMPAT_PCICLASS_CCSS,
	PROP_COMPAT_NUM,
};

static int of_pci_prop_device_type(struct pci_dev *pdev,
				   struct of_changeset *ocs,
				   struct device_node *np)
{
	return of_changeset_add_prop_string(ocs, np, "device_type", "pci");
}

static int of_pci_prop_address_cells(struct pci_dev *pdev,
				     struct of_changeset *ocs,
				     struct device_node *np)
{
	return of_changeset_add_prop_u32(ocs, np, "#address-cells",
					 OF_PCI_ADDRESS_CELLS);
}

static int of_pci_prop_size_cells(struct pci_dev *pdev,
				  struct of_changeset *ocs,
				  struct device_node *np)
{
	return of_changeset_add_prop_u32(ocs, np, "#size-cells",
					 OF_PCI_SIZE_CELLS);
}

static void of_pci_set_address(u32 *prop, u64 addr, u32 flags)
{
	prop[0] = flags;
	prop[1] = addr >> 32;
	prop[2] = addr;
}

static void of_pci_set_size(u32 *prop, u64 size)
{
	prop[0] = size >> 32;
	prop[1] = size;
}

static int of_pci_get_addr_flags(struct resource *res, u32 *flags)
{
	u32 ss;

	if (res->flags & IORESOURCE_IO)
		ss = OF_PCI_ADDR_SPACE_IO;
	else if (res->flags & IORESOURCE_MEM_64)
		ss = OF_PCI_ADDR_SPACE_MEM64;
	else if (res->flags & IORESOURCE_MEM)
		ss = OF_PCI_ADDR_SPACE_MEM32;
	else
		return -EINVAL;

	*flags &= ~(OF_PCI_ADDR_FIELD_SS | OF_PCI_ADDR_FIELD_PREFETCH);
	if (res->flags & IORESOURCE_PREFETCH)
		*flags |= OF_PCI_ADDR_FIELD_PREFETCH;

	*flags |= FIELD_PREP(OF_PCI_ADDR_FIELD_SS, ss);

	return 0;
}

static int of_pci_prop_ranges(struct pci_dev *pdev, struct of_changeset *ocs,
			      struct device_node *np)
{
	struct of_pci_range rp[PCI_BRIDGE_RESOURCE_NUM];
	struct resource *res;
	int i = 0, j, ret;
	u64 val64;
	u32 flags;

	res = &pdev->resource[PCI_BRIDGE_RESOURCES];
	for (j = 0; j < PCI_BRIDGE_RESOURCE_NUM; j++) {
		if (!resource_size(&res[j]))
			continue;

		flags = OF_PCI_ADDR_FIELD_NONRELOC;
		if (of_pci_get_addr_flags(&res[j], &flags))
			continue;

		val64 = res[j].start;

		of_pci_set_address(rp[i].parent_addr, val64, flags);
		of_pci_set_address(rp[i].child_addr, val64, flags);

		of_pci_set_size(rp[i].size, resource_size(&res[j]));

		i++;
	}

	ret = of_changeset_add_prop_u32_array(ocs, np, "ranges", (u32 *)rp,
					      i * sizeof(*rp) / sizeof(u32));

	return ret;
}

static int of_pci_prop_reg(struct pci_dev *pdev, struct of_changeset *ocs,
			   struct device_node *np)
{
	struct of_pci_addr_pair *reg;
	int i = 1, resno, ret = 0;
	u32 reg_val, base_addr;
	resource_size_t sz;

	reg = kzalloc(sizeof(*reg) * (PCI_STD_NUM_BARS + 1), GFP_KERNEL);
	if (!reg)
		return -ENOMEM;

	reg_val = FIELD_PREP(OF_PCI_ADDR_FIELD_SS, OF_PCI_ADDR_SPACE_CONFIG) |
		FIELD_PREP(OF_PCI_ADDR_FIELD_BUS, pdev->bus->number) |
		FIELD_PREP(OF_PCI_ADDR_FIELD_DEV, PCI_SLOT(pdev->devfn)) |
		FIELD_PREP(OF_PCI_ADDR_FIELD_FUNC, PCI_FUNC(pdev->devfn));
	of_pci_set_address(reg[0].phys_addr, 0, reg_val);

	base_addr = PCI_BASE_ADDRESS_0;
	for (resno = PCI_STD_RESOURCES; resno <= PCI_STD_RESOURCE_END;
	     resno++, base_addr += 4) {
		sz = pci_resource_len(pdev, resno);
		if (!sz)
			continue;

		ret = of_pci_get_addr_flags(&pdev->resource[resno], &reg_val);
		if (!ret)
			continue;

		reg_val &= ~OF_PCI_ADDR_FIELD_REG;
		reg_val |= FIELD_PREP(OF_PCI_ADDR_FIELD_REG, base_addr);
		of_pci_set_address(reg[i].phys_addr, 0, reg_val);
		of_pci_set_size(reg[i].size, sz);
		i++;
	}

	ret = of_changeset_add_prop_u32_array(ocs, np, "reg", (u32 *)reg,
					      i * sizeof(*reg) / sizeof(u32));
	kfree(reg);

	return ret;
}

static int of_pci_prop_compatible(struct pci_dev *pdev,
				  struct of_changeset *ocs,
				  struct device_node *np)
{
	const char *compat_strs[PROP_COMPAT_NUM] = { 0 };
	int i, ret;

	compat_strs[PROP_COMPAT_PCI_VVVV_DDDD] =
		kasprintf(GFP_KERNEL, "pci%x,%x", pdev->vendor, pdev->device);
	compat_strs[PROP_COMPAT_PCICLASS_CCSSPP] =
		kasprintf(GFP_KERNEL, "pciclass,%06x", pdev->class);
	compat_strs[PROP_COMPAT_PCICLASS_CCSS] =
		kasprintf(GFP_KERNEL, "pciclass,%04x", pdev->class >> 8);

	ret = of_changeset_add_prop_string_array(ocs, np, "compatible",
						 compat_strs, PROP_COMPAT_NUM);
	for (i = 0; i < PROP_COMPAT_NUM; i++)
		kfree(compat_strs[i]);

	return ret;
}

int of_pci_add_properties(struct pci_dev *pdev, struct of_changeset *ocs,
			  struct device_node *np)
{
	int ret = 0;

	if (pci_is_bridge(pdev)) {
		ret |= of_pci_prop_device_type(pdev, ocs, np);
		ret |= of_pci_prop_address_cells(pdev, ocs, np);
		ret |= of_pci_prop_size_cells(pdev, ocs, np);
		ret |= of_pci_prop_ranges(pdev, ocs, np);
	}

	ret |= of_pci_prop_reg(pdev, ocs, np);
	ret |= of_pci_prop_compatible(pdev, ocs, np);

	/*
	 * The added properties will be released when the
	 * changeset is destroyed.
	 */
	return ret;
}
