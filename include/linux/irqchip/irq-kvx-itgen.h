/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2018 Kalray Inc.
 */

#ifndef IRQ_KVX_ITGEN_H
#define IRQ_KVX_ITGEN_H



/* Parameters */
#define KVX_ITGEN_PARAM_OFFSET 0x1100
#define KVX_ITGEN_PARAM_IT_NUM_OFFSET     0x0

/* Target configuration */
#define KVX_ITGEN_CFG_ENABLE_OFFSET     0x8
#define KVX_ITGEN_CFG_ELEM_SIZE 0x10
#define KVX_ITGEN_CFG_TARGET_OFFSET     0x0
#define KVX_ITGEN_CFG_TARGET_MAILBOX_SHIFT    0x0
#define KVX_ITGEN_CFG_TARGET_MAILBOX_MASK     0x7FUL
#define KVX_ITGEN_CFG_TARGET_CLUSTER_SHIFT    0x8
#define KVX_ITGEN_CFG_TARGET_CLUSTER_MASK     0x700UL
#define KVX_ITGEN_CFG_TARGET_SELECT_BIT_SHIFT 0x18
#define KVX_ITGEN_CFG_TARGET_SELECT_BIT_MASK  0x3F000000UL

#endif /* IRQ_KVX_ITGEN_H */
