// SPDX-License-Identifier: GPL-2.0+
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/reset-controller.h>

#define CUPHY_REG_OFF	0x10
#define CUPHY_REG_BIT	0

struct lan966x_phy_reset_context {
	void __iomem *internal_phy_ctrl;
	struct gpio_desc *external_phy_ctrl;
	struct reset_controller_dev rcdev;
};

static int lan966x_phy_reset(struct reset_controller_dev *rcdev,
			     unsigned long id)
{
	struct lan966x_phy_reset_context *ctx =
		container_of(rcdev, struct lan966x_phy_reset_context, rcdev);
	u32 val;

	/* In case there are external PHYs toggle the GPIO to release the reset
	 * of the PHYs
	 */
	if (ctx->external_phy_ctrl) {
		gpiod_direction_output(ctx->external_phy_ctrl, 1);
		gpiod_set_value(ctx->external_phy_ctrl, 0);
		gpiod_set_value(ctx->external_phy_ctrl, 1);
		gpiod_set_value(ctx->external_phy_ctrl, 0);
	}

	/* Release the reset of internal PHY */
	val = readl(ctx->internal_phy_ctrl + CUPHY_REG_OFF);
	val |= BIT(CUPHY_REG_BIT);
	writel(val, ctx->internal_phy_ctrl + CUPHY_REG_OFF);

	return 0;
}

static const struct reset_control_ops lan966x_phy_reset_ops = {
	.reset = lan966x_phy_reset,
};

static int lan966x_phy_reset_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct lan966x_phy_reset_context *ctx;

	ctx = devm_kzalloc(&pdev->dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->internal_phy_ctrl = devm_platform_ioremap_resource_byname(pdev, "phy");
	if (IS_ERR(ctx->internal_phy_ctrl))
		return dev_err_probe(&pdev->dev, PTR_ERR(ctx->internal_phy_ctrl),
				     "Could not get resource 0\n");

	ctx->external_phy_ctrl = devm_gpiod_get_optional(&pdev->dev,
							 "external-phy-reset",
							 GPIOD_OUT_LOW);
	if (IS_ERR(ctx->external_phy_ctrl))
		return dev_err_probe(&pdev->dev, PTR_ERR(ctx->external_phy_ctrl),
				     "Could not get reset GPIO\n");

	ctx->rcdev.owner = THIS_MODULE;
	ctx->rcdev.nr_resets = 1;
	ctx->rcdev.ops = &lan966x_phy_reset_ops;
	ctx->rcdev.of_node = dn;
	ctx->rcdev.fwnode = dev_fwnode(&pdev->dev);

	return devm_reset_controller_register(&pdev->dev, &ctx->rcdev);
}

static const struct of_device_id lan966x_phy_reset_of_match[] = {
	{ .compatible = "microchip,lan966x-phy-reset", },
	{ }
};

static struct platform_driver lan966x_phy_reset_driver = {
	.probe = lan966x_phy_reset_probe,
	.driver = {
		.name = "lan966x-phy-reset",
		.of_match_table = lan966x_phy_reset_of_match,
	},
};
module_platform_driver(lan966x_phy_reset_driver);
MODULE_LICENSE("GPL");
