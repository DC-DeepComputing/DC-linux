/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2012 Regents of the University of California
 */

#ifndef _ASM_RISCV_PGTABLE_32_H
#define _ASM_RISCV_PGTABLE_32_H

#include <asm-generic/pgtable-nopmd.h>
#include <linux/bits.h>
#include <linux/const.h>

/* Size of region mapped by a page global directory */
#define PGDIR_SHIFT     22
#define PGDIR_SIZE      (_AC(1, UL) << PGDIR_SHIFT)
#define PGDIR_MASK      (~(PGDIR_SIZE - 1))

#define MAX_POSSIBLE_PHYSMEM_BITS 34

#define ALT_FIXUP_MT(_val)
#define ALT_UNFIX_MT(_val)

static inline bool pgtable_l4_enabled(void) { return false; }
static inline bool pgtable_l5_enabled(void) { return false; }

#endif /* _ASM_RISCV_PGTABLE_32_H */
