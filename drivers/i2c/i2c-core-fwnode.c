// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Linux I2C core fwnode support code
 *
 * Copyright (C) 2022 Microchip
 */

#include <linux/device.h>
#include <linux/i2c.h>

#include "i2c-core.h"

static int fwnode_dev_or_parent_node_match(struct device *dev, const void *data)
{
	if (device_match_fwnode(dev, data))
		return 1;

	/* For ACPI device node, we do not want to match the parent */
	if (!is_acpi_device_node(dev_fwnode(dev)) && dev->parent)
		return device_match_fwnode(dev->parent, data);

	return 0;
}

struct i2c_adapter *
fwnode_find_i2c_adapter_by_node(struct fwnode_handle *fwnode)
{
	struct device *dev;
	struct i2c_adapter *adapter;

	dev = bus_find_device(&i2c_bus_type, NULL, fwnode,
			      fwnode_dev_or_parent_node_match);
	if (!dev)
		return NULL;

	adapter = i2c_verify_adapter(dev);
	if (!adapter)
		put_device(dev);

	return adapter;
}
EXPORT_SYMBOL(fwnode_find_i2c_adapter_by_node);
