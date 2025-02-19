/* SPDX-License-Identifier: GPL-2.0 */
/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2018 Kalray Inc.
 */

#ifndef _ASM_KVX_PGALLOC_H
#define _ASM_KVX_PGALLOC_H

#include <linux/mm.h>
#include <asm/tlb.h>

#include <asm-generic/pgalloc.h>	/* for pte_{alloc,free}_one */

static inline void check_pgt_cache(void)
{
	/*
	 * check_pgt_cache() is called to check watermarks from counters that
	 * computes the number of pages allocated by cached allocation functions
	 * pmd_alloc_one_fast() and pte_alloc_one_fast().
	 * Currently we just skip this test.
	 */
}

/**
 * PGD
 */

static inline void
pgd_free(struct mm_struct *mm, pgd_t *pgd)
{
	free_pages((unsigned long) pgd, PAGES_PER_PGD);
}

static inline
pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd;

	pgd = (pgd_t *) __get_free_pages(GFP_KERNEL, PAGES_PER_PGD);
	if (unlikely(pgd == NULL))
		return NULL;

	memset(pgd, 0, USER_PTRS_PER_PGD * sizeof(pgd_t));

	/* Copy kernel mappings */
	memcpy(pgd + USER_PTRS_PER_PGD,
	       init_mm.pgd + USER_PTRS_PER_PGD,
	       (PTRS_PER_PGD - USER_PTRS_PER_PGD) * sizeof(pgd_t));

	return pgd;
}

/**
 * PUD
 */

static inline void pud_populate(struct mm_struct *mm, pud_t *pud, pmd_t *pmd)
{
	unsigned long pfn = virt_to_pfn(pmd);

	set_pud(pud, __pud((unsigned long)pfn << PAGE_SHIFT));
}

/**
 * PMD
 */

#define pmd_pgtable(pmd) pmd_page(pmd)
static inline void pmd_populate_kernel(struct mm_struct *mm,
	pmd_t *pmd, pte_t *pte)
{
	unsigned long pfn = virt_to_pfn(pte);

	set_pmd(pmd, __pmd((unsigned long)pfn << PAGE_SHIFT));
}

static inline void pmd_populate(struct mm_struct *mm,
	pmd_t *pmd, pgtable_t pte)
{
	unsigned long pfn = virt_to_pfn(page_address(pte));

	set_pmd(pmd, __pmd((unsigned long)pfn << PAGE_SHIFT));
}

#if CONFIG_PGTABLE_LEVELS > 2
#define __pmd_free_tlb(tlb, pmd, addr) pmd_free((tlb)->mm, pmd)

static inline pmd_t *pmd_alloc_one(struct mm_struct *mm, unsigned long addr)
{
	return (pmd_t *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
}

static inline void pmd_free(struct mm_struct *mm, pmd_t *pmd)
{
	free_page((unsigned long)pmd);
}

#endif /* CONFIG_PGTABLE_LEVELS > 2 */

/**
 * PTE
 */

#define __pte_free_tlb(tlb, pte, buf)   \
do {                                    \
	pgtable_pte_page_dtor(pte);         \
	tlb_remove_page((tlb), pte);    \
} while (0)

#endif /* _ASM_KVX_PGALLOC_H */
