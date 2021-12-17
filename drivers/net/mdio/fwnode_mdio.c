// SPDX-License-Identifier: GPL-2.0-only
/*
 * fwnode helpers for the MDIO (Ethernet PHY) API
 *
 * This file provides helper functions for extracting PHY device information
 * out of the fwnode and using it to populate an mii_bus.
 */

#include <linux/acpi.h>
#include <linux/acpi_mdio.h>
#include <linux/fwnode_mdio.h>
#include <linux/of.h>
#include <linux/of_mdio.h>
#include <linux/phy.h>

#define DEFAULT_GPIO_RESET_DELAY	10	/* in microseconds */

MODULE_AUTHOR("Calvin Johnson <calvin.johnson@oss.nxp.com>");
MODULE_LICENSE("GPL");

static struct mii_timestamper *
fwnode_find_mii_timestamper(struct fwnode_handle *fwnode)
{
	struct of_phandle_args arg;
	int err;

	if (is_acpi_node(fwnode) || is_software_node(fwnode))
		return NULL;

	err = of_parse_phandle_with_fixed_args(to_of_node(fwnode),
					       "timestamper", 1, 0, &arg);
	if (err == -ENOENT)
		return NULL;
	else if (err)
		return ERR_PTR(err);

	if (arg.args_count != 1)
		return ERR_PTR(-EINVAL);

	return register_mii_timestamper(arg.np, arg.args[0]);
}

int fwnode_mdiobus_phy_device_register(struct mii_bus *mdio,
				       struct phy_device *phy,
				       struct fwnode_handle *child, u32 addr)
{
	int rc;

	rc = fwnode_irq_get(child, 0);
	if (rc == -EPROBE_DEFER)
		return rc;

	if (rc > 0) {
		phy->irq = rc;
		mdio->irq[addr] = rc;
	} else {
		phy->irq = mdio->irq[addr];
	}

	if (fwnode_property_read_bool(child, "broken-turn-around"))
		mdio->phy_ignore_ta_mask |= 1 << addr;

	fwnode_property_read_u32(child, "reset-assert-us",
				 &phy->mdio.reset_assert_delay);
	fwnode_property_read_u32(child, "reset-deassert-us",
				 &phy->mdio.reset_deassert_delay);

	/* Associate the fwnode with the device structure so it
	 * can be looked up later
	 */
	fwnode_handle_get(child);
	device_set_node(&phy->mdio.dev, child);

	/* All data is now stored in the phy struct;
	 * register it
	 */
	rc = phy_device_register(phy);
	if (rc) {
		fwnode_handle_put(child);
		return rc;
	}

	dev_dbg(&mdio->dev, "registered phy %p fwnode at address %i\n",
		child, addr);
	return 0;
}
EXPORT_SYMBOL(fwnode_mdiobus_phy_device_register);

int fwnode_mdiobus_register_phy(struct mii_bus *bus,
				struct fwnode_handle *child, u32 addr)
{
	struct mii_timestamper *mii_ts = NULL;
	struct phy_device *phy;
	bool is_c45 = false;
	u32 phy_id;
	int rc;

	mii_ts = fwnode_find_mii_timestamper(child);
	if (IS_ERR(mii_ts))
		return PTR_ERR(mii_ts);

	rc = fwnode_property_match_string(child, "compatible",
					  "ethernet-phy-ieee802.3-c45");
	if (rc >= 0)
		is_c45 = true;

	if (is_c45 || fwnode_get_phy_id(child, &phy_id))
		phy = get_phy_device(bus, addr, is_c45);
	else
		phy = phy_device_create(bus, addr, phy_id, 0, NULL);
	if (IS_ERR(phy)) {
		unregister_mii_timestamper(mii_ts);
		return PTR_ERR(phy);
	}

	if (is_acpi_node(child)) {
		phy->irq = bus->irq[addr];

		/* Associate the fwnode with the device structure so it
		 * can be looked up later.
		 */
		phy->mdio.dev.fwnode = child;

		/* All data is now stored in the phy struct, so register it */
		rc = phy_device_register(phy);
		if (rc) {
			phy_device_free(phy);
			fwnode_handle_put(phy->mdio.dev.fwnode);
			return rc;
		}
	} else if (is_of_node(child) || is_software_node(child)) {
		rc = fwnode_mdiobus_phy_device_register(bus, phy, child, addr);
		if (rc) {
			unregister_mii_timestamper(mii_ts);
			phy_device_free(phy);
			return rc;
		}
	}

	/* phy->mii_ts may already be defined by the PHY driver. A
	 * mii_timestamper probed via the device tree will still have
	 * precedence.
	 */
	if (mii_ts)
		phy->mii_ts = mii_ts;
	return 0;
}
EXPORT_SYMBOL(fwnode_mdiobus_register_phy);

/* The following is a list of PHY compatible strings which appear in
 * some DTBs. The compatible string is never matched against a PHY
 * driver, so is pointless. We only expect devices which are not PHYs
 * to have a compatible string, so they can be matched to an MDIO
 * driver.  Encourage users to upgrade their DT blobs to remove these.
 */
static const struct of_device_id whitelist_phys[] = {
	{ .compatible = "brcm,40nm-ephy" },
	{ .compatible = "broadcom,bcm5241" },
	{ .compatible = "marvell,88E1111", },
	{ .compatible = "marvell,88e1116", },
	{ .compatible = "marvell,88e1118", },
	{ .compatible = "marvell,88e1145", },
	{ .compatible = "marvell,88e1149r", },
	{ .compatible = "marvell,88e1310", },
	{ .compatible = "marvell,88E1510", },
	{ .compatible = "marvell,88E1514", },
	{ .compatible = "moxa,moxart-rtl8201cp", },
	{}
};

/*
 * Return true if the child node is for a phy. It must either:
 * o Compatible string of "ethernet-phy-idX.X"
 * o Compatible string of "ethernet-phy-ieee802.3-c45"
 * o Compatible string of "ethernet-phy-ieee802.3-c22"
 * o No compatibility string
 *
 * A device which is not a phy is expected to have a compatible string
 * indicating what sort of device it is.
 */
bool fwnode_mdiobus_child_is_phy(struct fwnode_handle *child)
{
	u32 phy_id;

	if (fwnode_get_phy_id(child, &phy_id) != -EINVAL)
		return true;

	if (fwnode_is_compatible(child, "ethernet-phy-ieee802.3-c45"))
		return true;

	if (fwnode_is_compatible(child, "ethernet-phy-ieee802.3-c22"))
		return true;

	if (fwnode_match_node(child, whitelist_phys)) {
		pr_warn(FW_WARN
			"%s: Whitelisted compatible string. Please remove\n",
			fwnode_get_name(child));
		return true;
	}

	if (!fwnode_property_present(child, "compatible"))
		return true;

	return false;
}
EXPORT_SYMBOL(fwnode_mdiobus_child_is_phy);

static int fwnode_mdiobus_register_device(struct mii_bus *mdio,
					  struct fwnode_handle *child,
					  u32 addr)
{
	struct mdio_device *mdiodev;
	int rc;

	mdiodev = mdio_device_create(mdio, addr);
	if (IS_ERR(mdiodev))
		return PTR_ERR(mdiodev);

	fwnode_handle_get(child);
	device_set_node(&mdiodev->dev, child);

	/* All data is now stored in the mdiodev struct; register it. */
	rc = mdio_device_register(mdiodev);
	if (rc) {
		mdio_device_free(mdiodev);
		fwnode_handle_put(child);
		return rc;
	}

	dev_dbg(&mdio->dev, "registered mdio device %s at address %i\n",
		fwnode_get_name(child), addr);
	return 0;
}

static inline int fwnode_mdio_parse_addr(struct device *dev,
					 const struct fwnode_handle *fwnode)
{
	u32 addr;
	int ret;

	ret = fwnode_property_read_u32(fwnode, "reg", &addr);
	if (ret < 0) {
		dev_err(dev, "%s has invalid PHY address\n",
			fwnode_get_name(fwnode));
		return ret;
	}

	/* A PHY must have a reg property in the range [0-31] */
	if (addr >= PHY_MAX_ADDR) {
		dev_err(dev, "%s PHY address %i is too large\n",
			fwnode_get_name(fwnode), addr);
		return -EINVAL;
	}

	return addr;
}


/**
 * fwnode_mdiobus_register - Register mii_bus and create PHYs from fwnode
 * @mdio: pointer to mii_bus structure
 * @fwnode: pointer to fwnode of MDIO bus.
 *
 * This function returns of_mdiobus_register() for DT and
 * acpi_mdiobus_register() for ACPI.
 */
int fwnode_mdiobus_register(struct mii_bus *mdio, struct fwnode_handle *fwnode)
{
	struct fwnode_handle *child;
	bool scanphys = false;
	int addr, rc;

	if (!fwnode)
		return mdiobus_register(mdio);

	if (is_acpi_node(fwnode))
		return acpi_mdiobus_register(mdio, fwnode);

	if (!fwnode_device_is_available(fwnode))
		return -ENODEV;

	/* Mask out all PHYs from auto probing.  Instead the PHYs listed in
	 * the device tree are populated after the bus has been registered */
	mdio->phy_mask = ~0;

	device_set_node(&mdio->dev, fwnode);

	/* Get bus level PHY reset GPIO details */
	mdio->reset_delay_us = DEFAULT_GPIO_RESET_DELAY;
	fwnode_property_read_u32(fwnode, "reset-delay-us",
				 &mdio->reset_delay_us);
	mdio->reset_post_delay_us = 0;
	fwnode_property_read_u32(fwnode, "reset-post-delay-us",
				 &mdio->reset_post_delay_us);

	/* Register the MDIO bus */
	rc = mdiobus_register(mdio);
	if (rc)
		return rc;

	/* Loop over the child nodes and register a phy_device for each phy */
	fwnode_for_each_available_child_node(fwnode, child) {
		addr = fwnode_mdio_parse_addr(&mdio->dev, child);
		if (addr < 0) {
			scanphys = true;
			continue;
		}

		if (fwnode_mdiobus_child_is_phy(child))
			rc = fwnode_mdiobus_register_phy(mdio, child, addr);
		else
			rc = fwnode_mdiobus_register_device(mdio, child, addr);

		if (rc == -ENODEV)
			dev_err(&mdio->dev,
				"MDIO device at address %d is missing.\n",
				addr);
		else if (rc)
			goto unregister;
	}

	if (!scanphys)
		return 0;

	/* auto scan for PHYs with empty reg property */
	fwnode_for_each_available_child_node(fwnode, child) {
		/* Skip PHYs with reg property set */
		if (fwnode_property_present(child, "reg"))
			continue;

		for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
			/* skip already registered PHYs */
			if (mdiobus_is_registered_device(mdio, addr))
				continue;

			/* be noisy to encourage people to set reg property */
			dev_info(&mdio->dev, "scan phy %s at address %i\n",
				 fwnode_get_name(child), addr);

			if (fwnode_mdiobus_child_is_phy(child)) {
				/* -ENODEV is the return code that PHYLIB has
				 * standardized on to indicate that bus
				 * scanning should continue.
				 */
				rc = fwnode_mdiobus_register_phy(mdio, child,
								 addr);
				if (!rc)
					break;
				if (rc != -ENODEV)
					goto unregister;
			}
		}
	}

unregister:
	mdiobus_unregister(mdio);
	return rc;
}
EXPORT_SYMBOL(fwnode_mdiobus_register);
