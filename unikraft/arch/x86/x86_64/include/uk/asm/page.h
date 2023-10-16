/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Stefan Teodorescu <stefanl.teodorescu@gmail.com>
 *
 * Copyright (c) 2020, University Politehnica of Bucharest. All rights reserved.
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
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 *
 * Some of these macros here were inspired from Xen code.
 * For example, from "xen/include/asm-x86/x86_64/page.h" file.
 */
#include <uk/essentials.h>

/* TODO: fix duplicate definitions of these macros */
#define PAGE_SIZE		0x1000UL
#define PAGE_SHIFT		12
#define PAGE_MASK		(~(PAGE_SIZE - 1))

#define PAGE_LARGE_SIZE		0x200000UL
#define PAGE_LARGE_SHIFT	21
#define PAGE_LARGE_MASK		 (~(PAGE_LARGE_SIZE - 1))

#define PADDR_BITS		44
#define PADDR_MASK		((1UL << PADDR_BITS) - 1)

#define _PAGE_PRESENT	0x001UL
#define _PAGE_RW	0x002UL
#define _PAGE_USER	0x004UL
#define _PAGE_PWT	0x008UL
#define _PAGE_PCD	0x010UL
#define _PAGE_ACCESSED	0x020UL
#define _PAGE_DIRTY	0x040UL
#define _PAGE_PAT	0x080UL
#define _PAGE_PSE	0x080UL
#define _PAGE_GLOBAL	0x100UL
#define _PAGE_NX	(1UL << 63)
#define _PAGE_PROTNONE	(1UL << 58) /* one of the user available bits */

#if CONFIG_LIBFLEXOS_INTELPKU
#define _PAGE_PKEY0	(1UL << 59)
#define _PAGE_PKEY1	(1UL << 60)
#define _PAGE_PKEY2	(1UL << 61)
#define _PAGE_PKEY3	(1UL << 62)
#endif /* CONFIG_LIBFLEXOS_INTELPKU */

/*
 * If the user maps the page with PROT_NONE, the _PAGE_PRESENT bit is not set,
 * but PAGE_PRESENT must return true, so no other page is mapped on top.
 */
#define PAGE_PRESENT(vaddr)	((vaddr) & (_PAGE_PRESENT | _PAGE_PROTNONE))
#define PAGE_LARGE(vaddr)	((vaddr) & _PAGE_PSE)
#define PAGE_HUGE(vaddr)	((vaddr) & _PAGE_PSE)

#define L1_PROT    (_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_USER)
#define L1_PROT_RO (_PAGE_PRESENT | _PAGE_ACCESSED | _PAGE_USER)
#define L2_PROT    (_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER)
#define L3_PROT    (_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER)
#define L4_PROT    (_PAGE_PRESENT | _PAGE_RW | _PAGE_ACCESSED | _PAGE_DIRTY | _PAGE_USER)

/* round down to nearest page address */
#define PAGE_ALIGN_DOWN(vaddr)		ALIGN_DOWN(vaddr, PAGE_SIZE)
#define PAGE_LARGE_ALIGN_DOWN(vaddr)	ALIGN_DOWN(vaddr, PAGE_LARGE_SIZE)

/* round up to nearest page address */
#define PAGE_ALIGN_UP(vaddr)		ALIGN_UP(vaddr, PAGE_SIZE)
#define PAGE_LARGE_ALIGN_UP(vaddr)	ALIGN_UP(vaddr, PAGE_LARGE_SIZE)

#define PAGE_ALIGNED(vaddr)		(!((vaddr) & (PAGE_SIZE - 1)))
#define PAGE_LARGE_ALIGNED(vaddr)	(!((vaddr) & (PAGE_LARGE_SIZE - 1)))

#define PTE_REMOVE_FLAGS(pte)		(((pte) & PADDR_MASK) & PAGE_MASK)

/* Definitions for the API */
#define PAGE_PROT_NONE	0x0
#define PAGE_PROT_READ	0x1
#define PAGE_PROT_WRITE 0x2
#define PAGE_PROT_EXEC	0x4

#if CONFIG_LIBFLEXOS_INTELPKU
/* Memory Protection Keys, 4 bits, up to 16 keys */
#define PAGE_PROT_PKEY0	0x10	/* pkey bit 0 */
#define PAGE_PROT_PKEY1	0x20	/* pkey bit 1 */
#define PAGE_PROT_PKEY2	0x40	/* pkey bit 2 */
#define PAGE_PROT_PKEY3	0x80	/* pkey bit 3 */
#endif /* CONFIG_LIBFLEXOS_INTELPKU */

#define PAGE_FLAG_LARGE 0x1

#define PAGE_PADDR_ANY	((unsigned long) -1)

#define PAGE_INVALID	((unsigned long) -1)
#define PAGE_NOT_MAPPED 0
