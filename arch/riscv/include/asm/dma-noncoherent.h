/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2023 Renesas Electronics Corp.
 */

#ifndef __ASM_DMA_NONCOHERENT_H
#define __ASM_DMA_NONCOHERENT_H

#include <linux/dma-direct.h>

/*
 * struct riscv_nonstd_cache_ops - Structure for non-standard CMO function pointers
 *
 * @wback: Function pointer for cache writeback
 * @inv: Function pointer for invalidating cache
 * @wback_inv: Function pointer for flushing the cache (writeback + invalidating)
 */
struct riscv_nonstd_cache_ops {
	void (*wback)(phys_addr_t paddr, size_t size);
	void (*inv)(phys_addr_t paddr, size_t size);
	void (*wback_inv)(phys_addr_t paddr, size_t size);
};

extern struct riscv_nonstd_cache_ops noncoherent_cache_ops;

void riscv_noncoherent_register_cache_ops(const struct riscv_nonstd_cache_ops *ops);

#ifdef CONFIG_ARCH_ESWIN
#include <uapi/linux/es_vb_user.h>

typedef enum {
	FLAT_DDR_MEM = 0,
	INTERLEAVE_DDR_MEM,
	SPRAM,
} eic770x_memory_type_t;

void _do_arch_sync_cache_all(EIC770X_LOGICAL_MEM_NODE_E nid);
void arch_sync_cache_all(phys_addr_t phys, size_t size);

static inline void arch_get_mem_node_and_type(unsigned long pfn,
	EIC770X_LOGICAL_MEM_NODE_E *nid, eic770x_memory_type_t *p_mem_type)
{
	phys_addr_t phys = pfn_to_phys(pfn);

	#ifdef CONFIG_SOC_ESWIN_EIC7702
	if (unlikely((phys >= 0x59000000) && (phys < 0x59400000))) {
		*nid = EIC770X_LOGICAL_SPRAM_NODE_0;
		*p_mem_type = SPRAM;
	}
	else if (unlikely((phys >= 0x79000000) && (phys < 0x79400000))) {
		*nid = EIC770X_LOGICAL_SPRAM_NODE_1;
		*p_mem_type = SPRAM;
	}
	else if (unlikely((phys >= CONFIG_EIC7702_INTERLEAVE_CACHED_OFFSET) &&
			  (phys < (CONFIG_EIC7702_INTERLEAVE_CACHED_OFFSET + CONFIG_EIC7702_INTERLEAVE_MEM_MAX_SIZE)))){
		*p_mem_type = INTERLEAVE_DDR_MEM;
		*nid = EIC770X_LOGICAL_INTERLEAVE_MEM_NODE;
	} else {
		*p_mem_type = FLAT_DDR_MEM;
		*nid = pfn_to_nid(pfn);
	}
	#else
	if (unlikely((phys >= 0x59000000) && (phys < 0x59400000))) {
		*nid = EIC770X_LOGICAL_SPRAM_NODE_0;
		*p_mem_type = SPRAM;
	}
	else {
		*nid = EIC770X_LOGICAL_FLAT_MEM_NODE_0;
		*p_mem_type = FLAT_DDR_MEM;
	}
	#endif
}
#endif
#endif	/* __ASM_DMA_NONCOHERENT_H */
