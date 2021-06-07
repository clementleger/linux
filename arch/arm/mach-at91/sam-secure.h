/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012, Bootlin
 */

#ifndef SAM_SECURE_H
#define SAM_SECURE_H

#define SAMA5_API_MONITOR_BASE_INDEX	0x100

#define SAMA5_API_MON_VALUE(__val)	(SAMA5_API_MONITOR_BASE_INDEX + (__val))

/* Secure Monitor mode APIs */
#define SAMA5_MON_L2X0_WRITE_REG	SAMA5_API_MON_VALUE(1)

void __init sam_secure_init(void);
bool sam_linux_is_normal_world(void);
void sam_smccc_call(u32 fn, u32 arg0, u32 arg1);

#endif /* SAM_SECURE_H */