/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2019 Kalray Inc.
 */

#ifndef __ASM_KVX_INSN_H_
#define __ASM_KVX_INSN_H_

#include <linux/bits.h>

#ifndef __ASSEMBLY__
static inline int check_signed_imm(long long imm, int bits)
{
	long long min, max;

	min = -BIT_ULL(bits - 1);
	max = BIT_ULL(bits - 1) - 1;
	if (imm < min || imm > max)
		return 1;

	return 0;
}
#endif /* __ASSEMBLY__ */

#define BITMASK(bits)		(BIT_ULL(bits) - 1)

#define KVX_INSN_SYLLABLE_WIDTH 4

/* Values for general registers */
#define KVX_REG_R0	0
#define KVX_REG_R1	1
#define KVX_REG_R2	2
#define KVX_REG_R3	3
#define KVX_REG_R4	4
#define KVX_REG_R5	5
#define KVX_REG_R6	6
#define KVX_REG_R7	7
#define KVX_REG_R8	8
#define KVX_REG_R9	9
#define KVX_REG_R10	10
#define KVX_REG_R11	11
#define KVX_REG_R12	12
#define KVX_REG_SP	12
#define KVX_REG_R13	13
#define KVX_REG_TP	13
#define KVX_REG_R14	14
#define KVX_REG_FP	14
#define KVX_REG_R15	15
#define KVX_REG_R16	16
#define KVX_REG_R17	17
#define KVX_REG_R18	18
#define KVX_REG_R19	19
#define KVX_REG_R20	20
#define KVX_REG_R21	21
#define KVX_REG_R22	22
#define KVX_REG_R23	23
#define KVX_REG_R24	24
#define KVX_REG_R25	25
#define KVX_REG_R26	26
#define KVX_REG_R27	27
#define KVX_REG_R28	28
#define KVX_REG_R29	29
#define KVX_REG_R30	30
#define KVX_REG_R31	31
#define KVX_REG_R32	32
#define KVX_REG_R33	33
#define KVX_REG_R34	34
#define KVX_REG_R35	35
#define KVX_REG_R36	36
#define KVX_REG_R37	37
#define KVX_REG_R38	38
#define KVX_REG_R39	39
#define KVX_REG_R40	40
#define KVX_REG_R41	41
#define KVX_REG_R42	42
#define KVX_REG_R43	43
#define KVX_REG_R44	44
#define KVX_REG_R45	45
#define KVX_REG_R46	46
#define KVX_REG_R47	47
#define KVX_REG_R48	48
#define KVX_REG_R49	49
#define KVX_REG_R50	50
#define KVX_REG_R51	51
#define KVX_REG_R52	52
#define KVX_REG_R53	53
#define KVX_REG_R54	54
#define KVX_REG_R55	55
#define KVX_REG_R56	56
#define KVX_REG_R57	57
#define KVX_REG_R58	58
#define KVX_REG_R59	59
#define KVX_REG_R60	60
#define KVX_REG_R61	61
#define KVX_REG_R62	62
#define KVX_REG_R63	63

/* Value for bitfield parallel */
#define KVX_INSN_PARALLEL_EOB	0x0
#define KVX_INSN_PARALLEL_NONE	0x1

#define KVX_INSN_GOTO_SIZE 1
#define KVX_INSN_GOTO_PCREL27_CHECK(__val) \
	(((__val) & BITMASK(2)) || check_signed_imm((__val) >> 2, 27))
#define KVX_INSN_GOTO_MASK_0 0x78000000
#define KVX_INSN_GOTO_OPCODE_0 0x10000000
#define KVX_INSN_GOTO_SYLLABLE_0(__p, __pcrel27) \
	(KVX_INSN_GOTO_OPCODE_0 | ((__p) << 31) | (((__pcrel27) >> 2) & 0x7ffffff))
#define KVX_INSN_GOTO(__buf, __p, __pcrel27) \
do { \
	(__buf)[0] = KVX_INSN_GOTO_SYLLABLE_0(__p, __pcrel27); \
} while (0)

#define KVX_INSN_NOP_SIZE 1
#define KVX_INSN_NOP_MASK_0 0x7f03f000
#define KVX_INSN_NOP_OPCODE_0 0x7f03f000
#define KVX_INSN_NOP_SYLLABLE_0(__p) \
	(KVX_INSN_NOP_OPCODE_0 | ((__p) << 31))
#define KVX_INSN_NOP(__buf, __p) \
do { \
	(__buf)[0] = KVX_INSN_NOP_SYLLABLE_0(__p); \
} while (0)

#define KVX_INSN_SET_SIZE 1
#define KVX_INSN_SET_MASK_0 0x7ffc0000
#define KVX_INSN_SET_OPCODE_0 0xfc00000
#define KVX_INSN_SET_SYLLABLE_0(__p, __systemT3, __rz) \
	(KVX_INSN_SET_OPCODE_0 | ((__p) << 31) | (((__systemT3) & 0x1ff) << 6) | ((__rz) & 0x3f))
#define KVX_INSN_SET(__buf, __p, __systemT3, __rz) \
do { \
	(__buf)[0] = KVX_INSN_SET_SYLLABLE_0(__p, __systemT3, __rz); \
} while (0)

#endif /* __ASM_KVX_INSN_H_ */
