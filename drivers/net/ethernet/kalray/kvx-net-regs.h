/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2019 Kalray Inc.
 */

#ifndef ASM_KVX_ETH_REGS_H
#define ASM_KVX_ETH_REGS_H

#define KVX_ETH_JUMBO_MTU         (9216)
#define KVX_ETH_HEADER_SIZE       (32)
#define KVX_ETH_PARSER_NB         (32)

/* TX */
#define TX_OFFSET                                  0x15000
#define TX_LANE                                    0x800
#define TX_LANE_ELEM_SIZE                          0x20
#define TX_LANE_SA                                 0x0
#define TX_LANE_DA                                 0x8
#define TX_LANE_ETYPE                              0x10
#define TX_LANE_MTU                                0x14
#define TX_FIFO_OFFSET                             0x0
#define TX_FIFO_ELEM_SIZE                          0x40
#define TX_FIFO_NB                                 0xA
#define TX_FIFO_LANE_CTRL_OFFSET                   0x0
#define TX_FIFO_LANE_CTRL_ELEM_SIZE                0x4
#define TX_FIFO_LANE_CTRL_PAUSE_EN_SHIFT           0x0
#define TX_FIFO_LANE_CTRL_PAUSE_EN_MASK            0x1UL
#define TX_FIFO_LANE_CTRL_PFC_EN_SHIFT             0x1
#define TX_FIFO_LANE_CTRL_PFC_EN_MASK              0x1FEUL
#define TX_FIFO_LANE_CTRL_RR_TRIGGER_SHIFT         0x9
#define TX_FIFO_LANE_CTRL_RR_TRIGGER_MASK          0x1E00UL
#define TX_FIFO_CTRL_OFFSET                        0x10
#define TX_FIFO_CTRL_DROP_EN_SHIFT                 0x0
#define TX_FIFO_CTRL_DROP_EN_MASK                  0x1UL
#define TX_FIFO_CTRL_NOCX_EN_SHIFT                 0x1
#define TX_FIFO_CTRL_NOCX_EN_MASK                  0x2UL
#define TX_FIFO_CTRL_NOCX_PACK_EN_SHIFT            0x2
#define TX_FIFO_CTRL_NOCX_PACK_EN_MASK             0x4UL
#define TX_FIFO_CTRL_HEADER_EN_SHIFT               0x3
#define TX_FIFO_CTRL_HEADER_EN_MASK                0x8UL
#define TX_FIFO_CTRL_LANE_ID_SHIFT                 0x4
#define TX_FIFO_CTRL_LANE_ID_MASK                  0x30UL
#define TX_FIFO_CTRL_GLOBAL_SHIFT                  0x6
#define TX_FIFO_CTRL_GLOBAL_MASK                   0x40UL
#define TX_FIFO_CTRL_ASN_SHIFT                     0x7
#define TX_FIFO_CTRL_ASN_MASK                      0xFF80UL
#define TX_FIFO_XOFF_CTRL_OFFSET                   0x14
#define TX_FIFO_LEVEL_SHIFT                        0x0
#define TX_FIFO_LEVEL_MASK                         0xFFFFUL
#define TX_FIFO_XOFF_SHIFT                         0x10
#define TX_FIFO_XOFF_MASK                          0x10000UL
#define TX_FIFO_STATUS_OFFSET                      0x18
#define TX_FIFO_DROP_CNT_OFFSET                    0x1C

/* TX_NOC_IF */
#define TX_NOC_IF_OFFSET                           0xA00
#define TX_NOC_IF_ELEM_SIZE                        0x80
#define TX_NOC_IF_VCHAN_OFFSET                     0x0
#define TX_NOC_IF_VCHAN_ELEM_SIZE                  0x10
#define TX_NOC_IF_VCHAN_CTRL                       0x0
#define TX_NOC_IF_VCHAN_FIFO_MONITORING            0x4
#define TX_NOC_IF_VCHAN_CLR_LEVEL_MAX              0x8
#define TX_NOC_IF_PARITY_ERR_CNT                   0x20
#define TX_NOC_IF_CRC_ERR_CNT                      0x28
#define TX_NOC_IF_PERM_ERR_CNT                     0x30
#define TX_NOC_IF_FIFO_ERR_CNT                     0x38
#define TX_NOC_IF_NOC_PKT_DROP_CNT                 0x40

/* LB_DROP_CNT */
#define RX_LB_DROP_CNT_OFFSET                      0x12100
#define RX_LB_DROP_CNT_LANE_OFFSET                 0x0
#define RX_LB_DROP_CNT_LANE_ELEM_SIZE              0x40
#define RX_LB_DROP_CNT_LANE_MTU_OFFSET             0x0
#define RX_LB_DROP_CNT_LANE_FCS_OFFSET             0x8
#define RX_LB_DROP_CNT_LANE_FIFO_OFFSET            0x10
#define RX_LB_DROP_CNT_LANE_FIFO_CRC_OFFSET        0x18
#define RX_LB_DROP_CNT_LANE_TOTAL_OFFSET           0x20
#define RX_LB_DROP_CNT_LANE_RULE_OFFSET            0x28

/* RX */
#define RX_LB_DEFAULT_RULE_LANE_ARRAY_SIZE         0x4
#define RX_LB_DEFAULT_RULE_LANE_CTRL_OFFSET        0x0
#define RX_LB_DEFAULT_RULE_LANE_CTRL_DISPATCH_POLICY_SHIFT 0x0
#define RX_LB_DEFAULT_RULE_LANE_CTRL_DISPATCH_POLICY_MASK 0x3UL
#define RX_LB_DEFAULT_RULE_LANE_CTRL_RR_PKT_NB_SHIFT 0x8
#define RX_LB_DEFAULT_RULE_LANE_CTRL_RR_PKT_NB_MASK 0xFF00UL
#define RX_LB_DEFAULT_RULE_LANE_HIT_CNT_OFFSET     0x4
#define RX_LB_DEFAULT_RULE_OFFSET                  0x12300
#define RX_LB_DEFAULT_RULE_LANE_OFFSET             0x0
#define RX_LB_DEFAULT_RULE_LANE_ELEM_SIZE          0x40
#define RX_LB_DEFAULT_RULE_LANE_CTRL_OFFSET        0x0
#define RX_LB_CTRL_ARRAY_SIZE                      0x4
#define RX_LB_CTRL_MTU_SIZE_SHIFT                  0x0
#define RX_LB_CTRL_MTU_SIZE_MASK                   0x3FFFUL
#define RX_LB_CTRL_STORE_AND_FORWARD_SHIFT         0xe
#define RX_LB_CTRL_STORE_AND_FORWARD_MASK          0x4000UL
#define RX_LB_CTRL_KEEP_ALL_CRC_ERROR_PKT_SHIFT    0xf
#define RX_LB_CTRL_KEEP_ALL_CRC_ERROR_PKT_MASK     0x8000UL
#define RX_LB_CTRL_ADD_HEADER_SHIFT                0x10
#define RX_LB_CTRL_ADD_HEADER_MASK                 0x10000UL
#define RX_LB_CTRL_ADD_FOOTER_SHIFT                0x11
#define RX_LB_CTRL_ADD_FOOTER_MASK                 0x20000UL
#define RX_LB_OFFSET                               0x0
#define RX_LB_CTRL_OFFSET                          0x12400
#define RX_LB_CTRL_ELEM_SIZE                       0x4
#define RX_LB_DEFAULT_RULE_LANE_RR_TARGET_ARRAY_SIZE 0xA
#define RX_LB_DEFAULT_RULE_LANE_RR_TARGET_OFFSET   0x10
#define RX_LB_DEFAULT_RULE_LANE_RR_TARGET_ELEM_SIZE 0x4

/* RX_DISPATCH_TABLE */
#define RX_DISPATCH_TABLE_OFFSET                   0x14000
#define RX_DISPATCH_TABLE_ENTRY_OFFSET             0x0
#define RX_DISPATCH_TABLE_ENTRY_ARRAY_SIZE         0x140
#define RX_DISPATCH_TABLE_ENTRY_ELEM_SIZE          0x8
#define RX_DISPATCH_TABLE_ENTRY_NOC_ROUTE_SHIFT    0x0
#define RX_DISPATCH_TABLE_ENTRY_NOC_ROUTE_MASK     0xFFFFFFFFFF
#define RX_DISPATCH_TABLE_ENTRY_RX_CHAN_SHIFT      0x28
#define RX_DISPATCH_TABLE_ENTRY_RX_CHAN_MASK       0x3F0000000000
#define RX_DISPATCH_TABLE_ENTRY_NOC_VCHAN_SHIFT    0x2e
#define RX_DISPATCH_TABLE_ENTRY_NOC_VCHAN_MASK     0x400000000000
#define RX_DISPATCH_TABLE_ENTRY_ASN_SHIFT          0x2f
#define RX_DISPATCH_TABLE_ENTRY_ASN_MASK           0xFF800000000000
#define RX_DISPATCH_TABLE_ENTRY_SPLIT_EN_SHIFT     0x38
#define RX_DISPATCH_TABLE_ENTRY_SPLIT_EN_MASK      0x100000000000000
#define RX_DISPATCH_TABLE_ENTRY_SPLIT_TRIGGER_SHIFT 0x39
#define RX_DISPATCH_TABLE_ENTRY_SPLIT_TRIGGER_MASK 0xFE00000000000000

/* RX_LB_LUT */
#define RX_LB_LUT_OFFSET                           0x10000
#define RX_LB_LUT_LUT_OFFSET                       0x0
#define RX_LB_LUT_ARRAY_SIZE                       0x800
#define RX_LB_LUT_NOC_TABLE_ID_SHIFT               0x0
#define RX_LB_LUT_NOC_TABLE_ID_MASK                0x1FFUL
#define RX_LB_LUT_CTRL_OFFSET                      0x2000
#define RX_LB_LUT_CTRL_LANE_EN_SHIFT               0x0
#define RX_LB_LUT_CTRL_LANE_EN_MASK                0x3UL
#define RX_LB_LUT_CTRL_RULE_EN_SHIFT               0x2
#define RX_LB_LUT_CTRL_RULE_EN_MASK                0x7CUL
#define RX_LB_LUT_CTRL_PFC_EN_SHIFT                0x7
#define RX_LB_LUT_CTRL_PFC_EN_MASK                 0x380UL
#define RX_LB_LUT_QPN_CTRL_OFFSET                  0x2004
#define RX_LB_LUT_QPN_CTRL_QPN_EN_SHIFT            0x0
#define RX_LB_LUT_QPN_CTRL_QPN_EN_MASK             0xFFFFFFUL

/* RX_NOC_PKT */
#define RX_NOC_PKT_CTRL_OFFSET                     0x13900
#define RX_NOC_PKT_CTRL_ELEM_SIZE                  0x100
#define RX_NOC_PKT_CTRL_LANE_OFFSET                0x0
#define RX_NOC_PKT_CTRL_LANE_ELEM_SIZE             0x40
#define RX_NOC_PKT_CTRL_LANE_FDIR_VCHAN_OFFSET     0x0
#define RX_NOC_PKT_CTRL_LANE_FDIR_VCHAN_PPS_TIMER_SHIFT 0x0
#define RX_NOC_PKT_CTRL_LANE_FDIR_VCHAN_PPS_TIMER_MASK 0xFFFFUL
#define RX_NOC_PKT_CTRL_LANE_FDIR_VCHAN_PAYLOAD_FLIT_NB_MINUS1_SHIFT 0x10
#define RX_NOC_PKT_CTRL_LANE_FDIR_VCHAN_PAYLOAD_FLIT_NB_MINUS1_MASK 0xF0000UL

/* RX_LB_PARSER */
#define PARSER_CTRL_OFFSET                         0x12800
#define PARSER_CTRL_ELEM_SIZE                      0x40
#define PARSER_CTRL_CTL                            0x0
#define PARSER_CTRL_DISPATCH_POLICY_SHIFT          0x0
#define PARSER_CTRL_DISPATCH_POLICY_MASK           0x7UL
#define PARSER_CTRL_LANE_SRC_SHIFT                 0x3
#define PARSER_CTRL_LANE_SRC_MASK                  0x18UL
#define PARSER_CTRL_PRIO_SHIFT                     0x5
#define PARSER_CTRL_PRIO_MASK                      0xE0UL
#define PARSER_CTRL_RR_PKT_NB_SHIFT                0x8
#define PARSER_CTRL_RR_PKT_NB_MASK                 0xFF00UL
#define PARSER_CTRL_HASH_SEED_SHIFT                0x10
#define PARSER_CTRL_HASH_SEED_MASK                 0xFFFF0000UL
#define PARSER_CTRL_STATUS                         0x4
#define PARSER_CTRL_HIT_CNT                        0xC
#define PARSER_CTRL_RR_TARGET                      0x14
#define PARSER_CTRL_RR_TARGET_ELEM_SIZE            0x4
#define PARSER_CTRL_RR_TARGET_ARRAY_SIZE           0xA

/* RX_LB_PARSER_RAM */
#define PARSER_RAM_OFFSET                          0x0
#define PARSER_RAM_ELEM_SIZE                       0x800
#define PARSER_RAM_LINE                            0x0
#define PARSER_RAM_LINE_ELEM_SIZE                  0x40
#define PARSER_RAM_WORD                            0x0
#define PARSER_RAM_WORD_SIZE                       0x4
#define PARSER_RAM_WORD_NB                         0xD

/* RX_PFC */
#define RX_PFC_OFFSET                              0x13000
#define RX_PFC_LANE_OFFSET                         0x0
#define RX_PFC_LANE_CLASS_OFFSET                   0x0
#define RX_PFC_LANE_ELEM_SIZE                      0x200
#define RX_PFC_LANE_CLASS_ELEM_SIZE                0x20
#define RX_PFC_LANE_CLASS_RELEASE_LEVEL_OFFSET     0x4
#define RX_PFC_LANE_CLASS_RELEASE_LEVEL_SHIFT      0x0
#define RX_PFC_LANE_CLASS_RELEASE_LEVEL_MASK       0x7FFFFUL
#define RX_PFC_LANE_CLASS_DROP_LEVEL_SHIFT         0x0
#define RX_PFC_LANE_CLASS_DROP_LEVEL_MASK          0x7FFFFUL
#define RX_PFC_LANE_CLASS_DROP_LEVEL_OFFSET        0x8
#define RX_PFC_LANE_CLASS_ALERT_LEVEL_OFFSET       0x0
#define RX_PFC_LANE_CLASS_ALERT_LEVEL_SHIFT        0x0
#define RX_PFC_LANE_CLASS_ALERT_LEVEL_MASK         0x7FFFFUL
#define RX_PFC_LANE_CTRL_OFFSET                    0x12C
#define RX_PFC_LANE_CTRL_GLOBAL_PAUSE_EN_SHIFT     0x0
#define RX_PFC_LANE_CTRL_GLOBAL_PAUSE_EN_MASK      0x1UL
#define RX_PFC_LANE_CTRL_GLOBAL_PFC_EN_SHIFT       0x1
#define RX_PFC_LANE_CTRL_GLOBAL_PFC_EN_MASK        0x2UL
#define RX_PFC_LANE_CTRL_EN_SHIFT                  0x2
#define RX_PFC_LANE_CTRL_EN_MASK                   0x3FCUL
#define RX_PFC_LANE_GLOBAL_DROP_LEVEL_SHIFT        0x0
#define RX_PFC_LANE_GLOBAL_DROP_LEVEL_MASK         0x7FFFFUL
#define RX_PFC_LANE_GLOBAL_RELEASE_LEVEL_SHIFT     0x0
#define RX_PFC_LANE_GLOBAL_RELEASE_LEVEL_MASK      0x7FFFFUL
#define RX_PFC_LANE_GLOBAL_ALERT_LEVEL_SHIFT       0x0
#define RX_PFC_LANE_GLOBAL_ALERT_LEVEL_MASK        0x7FFFFUL
#define RX_PFC_LANE_GLOBAL_DROP_CNT_OFFSET         0x110
#define RX_PFC_LANE_GLOBAL_DROP_LEVEL_OFFSET       0x108
#define RX_PFC_LANE_GLOBAL_RELEASE_LEVEL_OFFSET    0x104
#define RX_PFC_LANE_GLOBAL_ALERT_LEVEL_OFFSET      0x100
#define RX_PFC_LANE_GLOBAL_NO_PFC_WMARK_OFFSET     0x120
#define RX_PFC_LANE_GLOBAL_NO_PFC_DROP_CNT_OFFSET  0x124

/* RX_PFC_IT */
#define RX_PFC_IT_OFFSET                           0x13800
#define RX_PFC_IT_LANE_OFFSET                      0x0
#define RX_PFC_IT_LANE_ELEM_SIZE                   0x20
#define RX_PFC_IT_LANE_PAUSE_CTRL_OFFSET           0xC
#define RX_PFC_IT_LANE_CTRL_XOFF_EN_SHIFT          0x0
#define RX_PFC_IT_LANE_CTRL_XOFF_EN_MASK           0xFFUL
#define RX_PFC_IT_LANE_CTRL_XOFF_OVERFLOW_EN_SHIFT 0x8
#define RX_PFC_IT_LANE_CTRL_XOFF_OVERFLOW_EN_MASK  0xFF00UL
#define RX_PFC_IT_LANE_CTRL_DROP_IT_EN_SHIFT       0x10
#define RX_PFC_IT_LANE_CTRL_DROP_IT_EN_MASK        0xFF0000UL
#define RX_PFC_IT_LANE_CTRL_DROP_OVERFLOW_IT_EN_SHIFT 0x18
#define RX_PFC_IT_LANE_CTRL_DROP_OVERFLOW_IT_EN_MASK 0xFF000000UL
#define RX_PFC_IT_LANE_PAUSE_CTRL_OFFSET           0xC

#endif /* ASM_KVX_ETH_REGS_H */
