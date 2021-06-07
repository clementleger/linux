/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012, Bootlin
 */

#ifndef SAM_SECURE_H
#define SAM_SECURE_H

#include <linux/arm-smccc.h>

/* Secure Monitor mode APIs */
#define SAMA5_SMC_SIP_L2X0_WRITE_REG	0x100

void __init sam_secure_init(void);
bool sam_linux_is_normal_world(void);
struct arm_smccc_res sam_smccc_call(u32 fn, u32 arg0, u32 arg1);

#endif /* SAM_SECURE_H */