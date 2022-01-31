/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Internal interface to pinctrl firmware node integration
 *
 * Copyright (C) 2021 Microchip
 */

#ifndef __PINCTRL_FWNODE_H
#define __PINCTRL_FWNODE_H

void pinctrl_fwnode_free_maps(struct pinctrl *p);
int pinctrl_fwnode_to_map(struct pinctrl *p, struct pinctrl_dev *pctldev);

#endif /* __PINCTRL_FWNODE_H */
