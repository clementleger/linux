// SPDX-License-Identifier: GPL-2.0-only
/*
 * Device tree integration for the pin control subsystem
 *
 * Copyright (C) 2012 NVIDIA CORPORATION. All rights reserved.
 */

#include <linux/device.h>
#include <linux/of.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/slab.h>

#include "core.h"
#include "devicetree.h"

/**
 * struct pinctrl_fwnode_map - mapping table chunk parsed from device tree
 * @node: list node for struct pinctrl's @maps field
 * @pctldev: the pin controller that allocated this struct, and will free it
 * @map: the mapping table entries
 * @num_maps: number of mapping table entries
 */
struct pinctrl_fwnode_map {
	struct list_head node;
	struct pinctrl_dev *pctldev;
	struct pinctrl_map *map;
	unsigned int num_maps;
};

static void fwnode_free_map(struct pinctrl_dev *pctldev,
			    struct pinctrl_map *map, unsigned int num_maps)
{
	int i;

	for (i = 0; i < num_maps; ++i) {
		kfree_const(map[i].dev_name);
		map[i].dev_name = NULL;
	}

	if (pctldev) {
		const struct pinctrl_ops *ops = pctldev->desc->pctlops;

		if (ops->dt_free_map)
			ops->dt_free_map(pctldev, map, num_maps);
	} else {
		/* There is no pctldev for PIN_MAP_TYPE_DUMMY_STATE */
		kfree(map);
	}
}

void pinctrl_fwnode_free_maps(struct pinctrl *p)
{
	struct pinctrl_fwnode_map *fwnode_map, *n1;

	list_for_each_entry_safe(fwnode_map, n1, &p->fwnode_maps, node) {
		pinctrl_unregister_mappings(fwnode_map->map);
		list_del(&fwnode_map->node);
		fwnode_free_map(fwnode_map->pctldev, fwnode_map->map,
			    fwnode_map->num_maps);
		kfree(fwnode_map);
	}

	fwnode_handle_put(dev_fwnode(p->dev));
}

static int fwnode_remember_or_free_map(struct pinctrl *p, const char *statename,
				       struct pinctrl_dev *pctldev,
				       struct pinctrl_map *map,
				       unsigned int num_maps)
{
	int i;
	struct pinctrl_fwnode_map *fwnode_map;

	/* Initialize common mapping table entry fields */
	for (i = 0; i < num_maps; i++) {
		const char *devname;

		devname = kstrdup_const(dev_name(p->dev), GFP_KERNEL);
		if (!devname)
			goto err_free_map;

		map[i].dev_name = devname;
		map[i].name = statename;
		if (pctldev)
			map[i].ctrl_dev_name = dev_name(pctldev->dev);
	}

	/* Remember the converted mapping table entries */
	fwnode_map = kzalloc(sizeof(*fwnode_map), GFP_KERNEL);
	if (!fwnode_map)
		goto err_free_map;

	fwnode_map->pctldev = pctldev;
	fwnode_map->map = map;
	fwnode_map->num_maps = num_maps;
	list_add_tail(&fwnode_map->node, &p->fwnode_maps);

	return pinctrl_register_mappings(map, num_maps);

err_free_map:
	fwnode_free_map(pctldev, map, num_maps);
	return -ENOMEM;
}

static int fwnode_to_map_one_config(struct pinctrl *p,
				struct pinctrl_dev *hog_pctldev,
				const char *statename,
				struct fwnode_handle *np_config)
{
	struct pinctrl_dev *pctldev = NULL;
	struct fwnode_handle *np_pctldev;
	const struct pinctrl_ops *ops;
	int ret;
	struct pinctrl_map *map;
	unsigned int num_maps;
	bool allow_default = false;

	/* Find the pin controller containing np_config */
	np_pctldev = fwnode_handle_get(np_config);
	for (;;) {
		if (!allow_default)
			allow_default = fwnode_property_read_bool(np_pctldev,
								  "pinctrl-use-default");

		np_pctldev = fwnode_get_next_parent(np_pctldev);
		if (!np_pctldev) {
			fwnode_handle_put(np_pctldev);
			ret = driver_deferred_probe_check_state(p->dev);
			/* keep deferring if modules are enabled */
			if (IS_ENABLED(CONFIG_MODULES) && !allow_default && ret < 0)
				ret = -EPROBE_DEFER;
			return ret;
		}
		/* If we're creating a hog we can use the passed pctldev */
		if (hog_pctldev && (np_pctldev == dev_fwnode(p->dev))) {
			pctldev = hog_pctldev;
			break;
		}
		pctldev = get_pinctrl_dev_from_fwnode(np_pctldev);
		if (pctldev)
			break;
		/* Do not defer probing of hogs (circular loop) */
		if (np_pctldev == dev_fwnode(p->dev)) {
			fwnode_handle_put(np_pctldev);
			return -ENODEV;
		}
	}
	fwnode_handle_put(np_pctldev);

	/*
	 * Call pinctrl driver to parse device tree node, and
	 * generate mapping table entries
	 */
	ops = pctldev->desc->pctlops;
	if (!ops->fwnode_to_map) {
		dev_err(p->dev, "pctldev %s doesn't support fwnode\n",
			dev_name(pctldev->dev));
		return -ENODEV;
	}
	ret = ops->fwnode_to_map(pctldev, np_config, &map, &num_maps);
	if (ret < 0)
		return ret;
	else if (num_maps == 0) {
		/*
		 * If we have no valid maps (maybe caused by empty pinctrl node
		 * or typing error) ther is no need remember this, so just
		 * return.
		 */
		dev_info(p->dev,
			 "there is not valid maps for state %s\n", statename);
		return 0;
	}

	/* Stash the mapping table chunk away for later use */
	return fwnode_remember_or_free_map(p, statename, pctldev, map, num_maps);
}

static int fwnode_remember_dummy_state(struct pinctrl *p, const char *statename)
{
	struct pinctrl_map *map;

	map = kzalloc(sizeof(*map), GFP_KERNEL);
	if (!map)
		return -ENOMEM;

	/* There is no pctldev for PIN_MAP_TYPE_DUMMY_STATE */
	map->type = PIN_MAP_TYPE_DUMMY_STATE;

	return fwnode_remember_or_free_map(p, statename, NULL, map, 1);
}

int pinctrl_fwnode_to_map(struct pinctrl *p, struct pinctrl_dev *pctldev)
{
	struct fwnode_handle *np = dev_fwnode(p->dev);
	int state, ret;
	char *propname;
	const char *statename;
	int config;
	struct fwnode_handle *np_config;

	if (!np)
		return 0;

	/* We may store pointers to property names within the node */
	fwnode_handle_get(np);

	/* For each defined state ID */
	for (state = 0; ; state++) {
		/* Retrieve the pinctrl-* property */
		propname = kasprintf(GFP_KERNEL, "pinctrl-%d", state);
		if (!fwnode_property_present(np, propname)) {
			if (state == 0) {
				fwnode_handle_put(np);
				return 0;
			}
			break;
		}

		/* Determine whether pinctrl-names property names the state */
		ret = fwnode_property_read_string_index(np, "pinctrl-names",
						    state, &statename);
		/*
		 * If not, statename is just the integer state ID. But rather
		 * than dynamically allocate it and have to free it later,
		 * just point part way into the property name for the string.
		 */
		if (ret < 0)
			statename = propname + strlen("pinctrl-");

		np_config = NULL;
		/* For every referenced pin configuration node in it */
		for (config = 0; ; config++) {
			np_config = fwnode_find_reference(np, propname, config);
			if (IS_ERR(np_config)) {
				np_config = NULL;
				break;
			}

			/* Parse the node */
			ret = fwnode_to_map_one_config(p, pctldev, statename,
						       np_config);
			fwnode_handle_put(np_config);
			if (ret < 0)
				goto err;
		}

		/* No entries in DT? Generate a dummy state table entry */
		if (config == 0 && !np_config) {
			ret = fwnode_remember_dummy_state(p, statename);
			if (ret < 0)
				goto err;
		}

		kfree(propname);
	}

	return 0;

err:
	kfree(propname);
	pinctrl_fwnode_free_maps(p);
	return ret;
}
