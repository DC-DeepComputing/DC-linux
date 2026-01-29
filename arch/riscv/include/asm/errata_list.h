/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Sifive.
 */
#ifndef ASM_ERRATA_LIST_H
#define ASM_ERRATA_LIST_H

#include <asm/csr.h>
#include <asm/insn-def.h>
#include <asm/hwcap.h>
#include <asm/vendorid_list.h>
#include <asm/errata_list_vendors.h>
#include <asm/vendor_extensions/mips.h>

#ifdef __ASSEMBLER__

#define ALT_INSN_FAULT(x)						\
ALTERNATIVE(__stringify(RISCV_PTR do_trap_insn_fault),			\
	    __stringify(RISCV_PTR sifive_cip_453_insn_fault_trp),	\
	    SIFIVE_VENDOR_ID, ERRATA_SIFIVE_CIP_453,			\
	    CONFIG_ERRATA_SIFIVE_CIP_453)

#define ALT_PAGE_FAULT(x)						\
ALTERNATIVE(__stringify(RISCV_PTR do_page_fault),			\
	    __stringify(RISCV_PTR sifive_cip_453_page_fault_trp),	\
	    SIFIVE_VENDOR_ID, ERRATA_SIFIVE_CIP_453,			\
	    CONFIG_ERRATA_SIFIVE_CIP_453)

#ifdef CONFIG_ERRATA_MIPS_P8700_WFI
#define ALT_WFI								\
ALTERNATIVE("wfi; .rept 7; nop; .endr;",				\
	".rept 8; .4byte 0x00501013; .endr;", MIPS_VENDOR_ID,		\
	ERRATA_MIPS_P8700_WFI, CONFIG_ERRATA_MIPS_P8700_WFI)
#else
#define ALT_WFI	wfi
#endif

#else /* !__ASSEMBLER__ */

#define ALT_SFENCE_VMA_ASID(asid)					\
asm(ALTERNATIVE("sfence.vma x0, %0", "sfence.vma", SIFIVE_VENDOR_ID,	\
		ERRATA_SIFIVE_CIP_1200, CONFIG_ERRATA_SIFIVE_CIP_1200)	\
		: : "r" (asid) : "memory")

#define ALT_SFENCE_VMA_ADDR(addr)					\
asm(ALTERNATIVE("sfence.vma %0", "sfence.vma", SIFIVE_VENDOR_ID,	\
		ERRATA_SIFIVE_CIP_1200, CONFIG_ERRATA_SIFIVE_CIP_1200)	\
		: : "r" (addr) : "memory")

#define ALT_SFENCE_VMA_ADDR_ASID(addr, asid)				\
asm(ALTERNATIVE("sfence.vma %0, %1", "sfence.vma", SIFIVE_VENDOR_ID,	\
		ERRATA_SIFIVE_CIP_1200, CONFIG_ERRATA_SIFIVE_CIP_1200)	\
		: : "r" (addr), "r" (asid) : "memory")

#define ALT_RISCV_PAUSE()					\
asm(ALTERNATIVE(	\
		RISCV_PAUSE, /* Original RISC‑V pause insn */	\
		MIPS_PAUSE, /* Replacement for MIPS P8700 */	\
		MIPS_VENDOR_ID, /* Vendor ID to match */	\
		ERRATA_MIPS_P8700_PAUSE_OPCODE, /* patch_id */	\
		CONFIG_ERRATA_MIPS_P8700_PAUSE_OPCODE)	\
	: /* no outputs */	\
	: /* no inputs */	\
	: "memory")

#ifdef CONFIG_ERRATA_MIPS_P8700_WFI
/*
 * A WFI instruction after a mispredicted branch can result in erroneous address translation
 * on the MIPS P8700.
 * The number of MIPS_PAUSE instructions required was determined by measuring
 * Coremark performance on a one-core, two-hart configuration. Eight PAUSE instructions matched
 * the latency of a single WFI instruction.
 * Usually, the maximum number of cycles per PAUSE instruction is 63. Exceptionally, if the hart's
 * LR reservation bit is set, the maximum number of cycles is extended to 255
 * (and the hart is woken up once it is cleared). This case should not affect the patched sites.
 */
#define ALT_RISCV_WFI()										\
asm volatile(ALTERNATIVE(									\
		"wfi\n" /* Original RISC-V wfi insn */						\
		__nops(7),									\
		".rept 8;" MIPS_PAUSE ".endr;\n", /* Replacement: mips.pause for P8700 */	\
		MIPS_VENDOR_ID, /* Vendor ID to match */					\
		ERRATA_MIPS_P8700_WFI, /* patch_id */						\
		CONFIG_ERRATA_MIPS_P8700_WFI)							\
		: /* no outputs */								\
		: /* no inputs */								\
		: "memory")
#else
#define ALT_RISCV_WFI()		\
	__asm__ __volatile__ ("wfi")
#endif

#define ALT_CMO_OP(_op, _start, _size, _cachesize)			\
asm volatile(ALTERNATIVE(						\
	__nops(5),							\
	"mv a0, %1\n\t"							\
	"j 2f\n\t"							\
	"3:\n\t"							\
	CBO_##_op(a0)							\
	"add a0, a0, %0\n\t"						\
	"2:\n\t"							\
	"bltu a0, %2, 3b\n\t",						\
	0, RISCV_ISA_EXT_ZICBOM, CONFIG_RISCV_ISA_ZICBOM)		\
	: : "r"(_cachesize),						\
	    "r"((unsigned long)(_start) & ~((_cachesize) - 1UL)),	\
	    "r"((unsigned long)(_start) + (_size))			\
	: "a0")

#define THEAD_C9XX_RV_IRQ_PMU			17
#define THEAD_C9XX_CSR_SCOUNTEROF		0x5c5

#endif /* __ASSEMBLER__ */

#endif
