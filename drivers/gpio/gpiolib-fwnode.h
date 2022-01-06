/* SPDX-License-Identifier: GPL-2.0 */

#ifndef GPIOLIB_FWNODE_H
#define GPIOLIB_FWNODE_H

struct gpio_chip;
struct device;
struct gpio_desc;

int fwnode_gpiochip_add(struct gpio_chip *chip);

struct gpio_desc *fwnode_find_gpio(const struct fwnode_handle *node,
				   const char *con_id, unsigned int idx,
				   unsigned long *flags);

#endif /* GPIOLIB_FWNODE_H */
