/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Stefan Teodorescu <stefanl.teodorescu@gmail.com>
 *
 * Copyright (c) 2021, University Politehnica of Bucharest. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Some of these macros here were inspired from Xen code.
 * For example, from "xen/include/asm-x86/x86_64/page.h" file.
 */

#ifndef __UKARCH_X86_64_MM_NATIVE__
#define __UKARCH_X86_64_MM_NATIVE__

#include "mm.h"
#include <uk/bitmap.h>
#include <uk/assert.h>
#include <uk/print.h>

#include <uk/mem_layout.h>

#define pt_pte_to_virt(pte) (PTE_REMOVE_FLAGS(pte) + _virt_offset)
#define pt_virt_to_mfn(vaddr) ((vaddr - _virt_offset) >> PAGE_SHIFT)
#define pfn_to_mfn(pfn) (pfn)

#define pte_to_pfn(pte) (PTE_REMOVE_FLAGS(pte) >> PAGE_SHIFT)
#define pfn_to_mframe(pfn) (pfn << PAGE_SHIFT)
#define mframe_to_pframe(mframe) (mframe)

static inline unsigned long ukarch_read_pt_base(void)
{
	unsigned long cr3;

	__asm__ __volatile__("movq %%cr3, %0" : "=r"(cr3)::);

	/*
	 * For consistency with Xen implementation, which returns a virtual
	 * address, this should return the same.
	 */
	return pt_pte_to_virt(cr3);
}

static inline void ukarch_write_pt_base(unsigned long cr3)
{
	__asm__ __volatile__("movq %0, %%cr3" :: "r"(cr3) : );
}

static inline int ukarch_flush_tlb_entry(unsigned long vaddr)
{
	__asm__ __volatile__("invlpg (%0)" ::"r" (vaddr) : "memory");

	return 0;
}

static inline int ukarch_pte_write(unsigned long pt, size_t offset,
		unsigned long val, size_t level)
{
	return _ukarch_pte_write_raw(pt, offset, val, level);
}

#endif	/* __UKARCH_X86_64_MM_NATIVE__ */
