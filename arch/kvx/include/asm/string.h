/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2018 Kalray Inc.
 */

#ifndef _ASM_KVX_STRING_H
#define _ASM_KVX_STRING_H

#define __HAVE_ARCH_MEMSET
extern void *memset(void *s, int c, size_t n);

#define __HAVE_ARCH_MEMCPY
extern void *memcpy(void *dest, const void *src, size_t n);

#define __HAVE_ARCH_STRLEN
extern size_t strlen(const char *s);

#endif	/* _ASM_KVX_STRING_H */
