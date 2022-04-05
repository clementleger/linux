// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2022 Schneider-Electric
 *
 * Clément Léger <clement.leger@bootlin.com>
 */

#include <linux/of.h>
#include <linux/pcs-rzn1-miic.h>
#include <linux/phylink.h>
#include <linux/platform_device.h>

#include "stmmac_platform.h"
#include "stmmac.h"

struct rzn1_dwmac {
	struct phylink_pcs *pcs;
};

static int rzn1_dt_parse(struct device *dev, struct rzn1_dwmac *dwmac)
{
	struct device_node *np = dev->of_node;
	struct device_node *pcs_node;
	struct phylink_pcs *pcs;

	pcs_node = of_parse_phandle(np, "pcs-handle", 0);
	if (!pcs_node)
		return 0;

	pcs = miic_create(dev, pcs_node);
	if (IS_ERR(pcs))
		return PTR_ERR(pcs);

	dwmac->pcs = pcs;

	return 0;
}

static int rzn1_dwmac_probe(struct platform_device *pdev)
{
	struct plat_stmmacenet_data *plat_dat;
	struct stmmac_resources stmmac_res;
	struct device *dev = &pdev->dev;
	struct rzn1_dwmac *dwmac;
	int ret;

	ret = stmmac_get_platform_resources(pdev, &stmmac_res);
	if (ret)
		return ret;

	plat_dat = stmmac_probe_config_dt(pdev, stmmac_res.mac);
	if (IS_ERR(plat_dat))
		return PTR_ERR(plat_dat);

	dwmac = devm_kzalloc(dev, sizeof(*dwmac), GFP_KERNEL);
	if (!dwmac) {
		ret = -ENOMEM;
		goto err_remove_config_dt;
	}

	ret = rzn1_dt_parse(dev, dwmac);
	if (ret)
		goto err_remove_config_dt;

	plat_dat->bsp_priv = dwmac;
	plat_dat->pcs = dwmac->pcs;

	ret = stmmac_dvr_probe(dev, plat_dat, &stmmac_res);
	if (ret)
		goto err_free_pcs;

	return 0;

err_free_pcs:
	if (dwmac->pcs)
		miic_destroy(dwmac->pcs);

err_remove_config_dt:
	stmmac_remove_config_dt(pdev, plat_dat);

	return ret;
}

static int rzn1_dwmac_remove(struct platform_device *pdev)
{
	struct rzn1_dwmac *dwmac = get_stmmac_bsp_priv(&pdev->dev);
	int ret = stmmac_dvr_remove(&pdev->dev);

	if (dwmac->pcs)
		miic_destroy(dwmac->pcs);

	return ret;
}


static const struct of_device_id rzn1_dwmac_match[] = {
	{ .compatible = "renesas,rzn1-gmac" },
	{ }
};
MODULE_DEVICE_TABLE(of, rzn1_dwmac_match);

static struct platform_driver rzn1_dwmac_driver = {
	.probe  = rzn1_dwmac_probe,
	.remove = rzn1_dwmac_remove,
	.driver = {
		.name           = "rzn1-dwmac",
		.of_match_table = rzn1_dwmac_match,
	},
};
module_platform_driver(rzn1_dwmac_driver);

MODULE_AUTHOR("Clément Léger <clement.leger@bootlin.com>");
MODULE_DESCRIPTION("Renesas RZN1 DWMAC specific glue layer");
MODULE_LICENSE("GPL");
