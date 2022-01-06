// SPDX-License-Identifier: GPL-2.0+
/*
 * fwnode helpers for the GPIO API
 *
 * Copyright (c) Microchip UNG
 *
 * Author: Clément Léger <clement.leger@bootlin.fr>
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/property.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>

#include "gpiolib.h"
#include "gpiolib-fwnode.h"

static int fwnode_gpiochip_match_node_and_xlate(struct gpio_chip *chip, void *data)
{
	struct fwnode_reference_args *args = data;

	return dev_fwnode(&chip->gpiodev->dev) == args->fwnode &&
				chip->fwnode_xlate &&
				chip->fwnode_xlate(chip, args, NULL) >= 0;
}

static struct gpio_chip *fwnode_find_gpiochip_by_xlate(
					struct fwnode_reference_args *args)
{
	return gpiochip_find(args, fwnode_gpiochip_match_node_and_xlate);
}


static struct gpio_desc *fwnode_xlate_and_get_gpiod_flags(struct gpio_chip *chip,
					struct fwnode_reference_args *args,
					unsigned long *flags)
{
	int ret;

	if (chip->fwnode_gpio_n_cells != args->nargs)
		return ERR_PTR(-EINVAL);

	ret = chip->fwnode_xlate(chip, args, flags);
	if (ret < 0)
		return ERR_PTR(ret);

	return gpiochip_get_desc(chip, ret);
}


static struct gpio_desc *fwnode_get_named_gpiod_flags(
		     const struct fwnode_handle *node,
		     const char *propname, int index, unsigned long  *flags)
{
	struct fwnode_reference_args args;
	struct gpio_chip *chip;
	struct gpio_desc *desc;
	int ret;

	ret = fwnode_property_get_reference_args(node, propname, "#gpio-cells", 0,
						 index, &args);
	if (ret) {
		pr_debug("%s: can't parse '%s' property of fwnode '%s[%d]: %d'\n",
			__func__, propname, fwnode_get_name(node), index, ret);
		return ERR_PTR(ret);
	}

	chip = fwnode_find_gpiochip_by_xlate(&args);
	if (!chip) {
		desc = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	desc = fwnode_xlate_and_get_gpiod_flags(chip, &args, flags);
	if (IS_ERR(desc))
		goto out;

	pr_debug("%s: parsed '%s' property of node '%s[%d]' - status (%d), %lx\n",
		 __func__, propname, fwnode_get_name(node), index,
		 PTR_ERR_OR_ZERO(desc), *flags);

out:
	fwnode_handle_put(args.fwnode);

	return desc;
}


struct gpio_desc *fwnode_find_gpio(const struct fwnode_handle *node,
				   const char *con_id, unsigned int idx,
				   unsigned long *flags)
{
	char prop_name[32] = "gpios"; /* 32 is max size of property name */

	if (!node)
		return NULL;

	if (con_id)
		snprintf(prop_name, sizeof(prop_name), "%s-gpios", con_id);

	return fwnode_get_named_gpiod_flags(node, prop_name, idx, flags);
}

/**
 * fwnode_gpio_simple_xlate - translate fwnode_reference_args to the GPIO number
 *			      and flags
 * @gc:		pointer to the gpio_chip structure
 * @args:	fwnode node GPIO args
 * @flags:	a flags pointer to fill in
 *
 * This is simple translation function, suitable for the most 1:1 mapped
 * GPIO chips. This function performs only one sanity check: whether GPIO
 * is less than ngpios (that is specified in the gpio_chip).
 */
static int fwnode_gpio_simple_xlate(struct gpio_chip *gc,
				    const struct fwnode_reference_args *args,
				    unsigned long *flags)
{
	/*
	 * We're discouraging gpio_cells < 2, since that way you'll have to
	 * write your own xlate function (that will have to retrieve the GPIO
	 * number and the flags from a single gpio cell -- this is possible,
	 * but not recommended).
	 */
	if (gc->fwnode_gpio_n_cells < 2) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (WARN_ON(args->nargs < gc->fwnode_gpio_n_cells))
		return -EINVAL;

	if (args->args[0] >= gc->ngpio)
		return -EINVAL;

	if (flags)
		*flags = args->args[1];

	return args->args[0];
}

#ifdef CONFIG_PINCTRL
static int fwnode_gpiochip_add_pin_range(struct gpio_chip *chip)
{
	struct fwnode_handle *node = dev_fwnode(&chip->gpiodev->dev);
	struct fwnode_reference_args pinargs;
	struct pinctrl_dev *pctldev;
	int index = 0, ret;
	const char *name;
	static const char group_names_propname[] = "gpio-ranges-group-names";
	const char **group_names = NULL;
	int group_names_cnt = 0;

	if (!node)
		return 0;

	group_names_cnt = fwnode_property_string_array_count(node, group_names_propname);
	if (group_names_cnt > 0) {
		group_names = kcalloc(group_names_cnt, sizeof(char *),
				      GFP_KERNEL);
		if (!group_names)
			return -ENOMEM;

		ret = fwnode_property_read_string_array(node,
							group_names_propname,
							group_names,
							group_names_cnt);
		if (ret)
			goto out_err;
	}

	for (;; index++) {
		ret = fwnode_property_get_reference_args(node, "gpio-ranges",
							 NULL, 3, index,
							 &pinargs);
		if (ret)
			break;

		pctldev = fwnode_pinctrl_get(pinargs.fwnode);
		fwnode_handle_put(pinargs.fwnode);
		if (!pctldev) {
			ret = -EPROBE_DEFER;
			goto out_err;
		}

		if (pinargs.args[2]) {
			if (group_names) {
				name = group_names[index];
				if (strlen(name)) {
					pr_err("%s: Group name of numeric GPIO ranges must be the empty string.\n",
						fwnode_get_name(node));
					break;
				}
			}
			/* npins != 0: linear range */
			ret = gpiochip_add_pin_range(chip,
					pinctrl_dev_get_devname(pctldev),
					pinargs.args[0],
					pinargs.args[1],
					pinargs.args[2]);
			if (ret)
				goto out_err;
		} else {
			/* npins == 0: special range */
			if (pinargs.args[1]) {
				pr_err("%s: Illegal gpio-range format.\n",
					fwnode_get_name(node));
				break;
			}

			if (!group_names) {
				pr_err("%s: GPIO group range requested but no %s property.\n",
					fwnode_get_name(node),
					group_names_propname);
				break;
			}

			if (index > group_names_cnt)
				break;

			name = group_names[index];
			if (!strlen(name)) {
				pr_err("%s: Group name of GPIO group range cannot be the empty string.\n",
				       fwnode_get_name(node));
				break;
			}

			ret = gpiochip_add_pingroup_range(chip, pctldev,
						pinargs.args[0], name);
			if (ret)
				goto out_err;
		}
	}

	return 0;

out_err:
	kfree(group_names);

	return ret;
}

#else
static int fwnode_gpiochip_add_pin_range(struct gpio_chip *chip) { return 0; }
#endif

int fwnode_gpiochip_add(struct gpio_chip *chip)
{
	int ret;
	struct fwnode_handle *node = dev_fwnode(&chip->gpiodev->dev);

	if (!chip->fwnode_xlate) {
		chip->fwnode_gpio_n_cells = 2;
		chip->fwnode_xlate = fwnode_gpio_simple_xlate;
	}

	ret = fwnode_gpiochip_add_pin_range(chip);
	if (ret)
		return ret;

	fwnode_handle_get(node);

	return ret;
}
