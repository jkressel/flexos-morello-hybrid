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
 */

#ifndef __UK_MEM_LAYOUT__
#define __UK_MEM_LAYOUT__

#include <uk/arch/limits.h>
#include <uk/sections.h>

/* These regions exist only for KVM and are mapped 1:1 */
#ifdef CONFIG_PLAT_KVM
/*
 * The VGA buffer is always mapped at this physical address (0xb8000)
 * (https://wiki.osdev.org/Printing_To_Screen) and we map it at the same
 * virtual address for convenience.
 */
#define VGABUFFER_AREA_START	0xb8000
#define VGABUFFER_AREA_END	0xc0000
#define VGABUFFER_AREA_SIZE	(VGABUFFER_AREA_END - VGABUFFER_AREA_START)

/*
 * This area is reserved by QEMU for the multiboot info struct
 * (https://github.com/qemu/qemu/blob/master/hw/i386/multiboot.c#L44)
 * See more about this struct in in plat/kvm/include/kvm-x86/multiboot.h
 */
#define MBINFO_AREA_START	0x9000
#define MBINFO_AREA_END		0xa000
#define MBINFO_AREA_SIZE	(MBINFO_AREA_END - MBINFO_AREA_START)
#endif /* CONFIG_PLAT_KVM */

#ifdef CONFIG_PARAVIRT
#define SHAREDINFO_PAGE		0x1000
#endif /* CONFIG_PARAVIRT */

/*
 * This is the area where the kernel binary is mapped, starting from 1MB in the
 * virtual space.
 * Here are the regions: Code + Data + BSS + Rodata etc.
 *
 * TODO: This has to be broken down further into the composing regions:
 * Code   - R-X
 * Data   - RW-
 * Rodata - R--
 * etc.
 */
#define KERNEL_AREA_START	(1UL << 20) /* 1MB */
#define KERNEL_AREA_END		PAGE_LARGE_ALIGN_UP(__END)
#define KERNEL_AREA_SIZE	(KERNEL_AREA_END - KERNEL_AREA_START)

/*
 * The virtual memory area for storing the page tables. The size is hardcoded
 * for now to 16MB. TODO: figure out a way to dynamically alloc this.
 */
#define BOOKKEEP_AREA_START	(1UL << 32) /* 4GB */
#define BOOKKEEP_AREA_END	(BOOKKEEP_AREA_START + 0x1000000) /* 16MB */
#define BOOKKEEP_AREA_SIZE	(BOOKKEEP_AREA_END - BOOKKEEP_AREA_START)

/*
 * TODO this number has depends on how much memory we have
 * On Xen it can't be too high because not much memory is initially mapped.
 */
#define PAGETABLES_AREA_START	(BOOKKEEP_AREA_START + 0x200000) /* 2MB */
#define PAGETABLES_AREA_END	BOOKKEEP_AREA_END
#define PAGETABLES_AREA_SIZE	(PAGETABLES_AREA_END - PAGETABLES_AREA_START)

/*
 * The virtual memory area where all stacks will be allocated from. It is now
 * hardcoded for 256 stacks. These are not actually mapped from the beginning,
 * they are allocated as they are needed. The number of available stacks
 * directly gives the number of threads that can be created (one stack per
 * thread). TODO: check if this number needs to be increased or dynamically
 * calculated.
 */
#define STACK_COUNT		256
#define STACK_AREA_END		(1UL << 47) /* 128TB */
#define STACK_AREA_START	(STACK_AREA_END - STACK_COUNT * __STACK_SIZE)
#define STACK_AREA_SIZE		(STACK_COUNT * __STACK_SIZE)

/*
 * This is a general use area that is reserved to be used when creating
 * mappings with the internal API, for example by drivers that need to create
 * mappings with page granularity for IO, or any other pages by the kernel.
 */
#define MAPPINGS_AREA_START	(1UL << 33) /* 8GB */
#define MAPPINGS_AREA_END	(1UL << 34) /* 16GB */
#define MAPPINGS_AREA_SIZE	(MAPPINGS_AREA_END - MAPPINGS_AREA_START)

/*
 * Next are the heap and the mmap areas.
 *
 * The heap memory is the one managed by the memory allocator(s).
 *
 * If the POSIX mmap library is included, the chunk of virtual memory
 * address space is divided between the heap and the mmap area, with 32TB for
 * the heap and the rest ~96TB for mmap. When an mmap() call is done, the
 * returned address is in this area (calls with MAP_FIXED outside of this area
 * will fail).
 *
 * If POSIX mmap is not included, this chunk between the general use mappings
 * area and the stack is reserved for the heap.
 *
 * Immediately after these areas is the heap, at the end of the virtual
 * address space.
 */
#define HEAP_AREA_START		MAPPINGS_AREA_END
#ifdef CONFIG_LIBPOSIX_MMAP
#define HEAP_AREA_END		(1UL << 45) /* 32TB */
#define HEAP_AREA_SIZE		(HEAP_AREA_END - HEAP_AREA_START)

#define MMAP_AREA_START		HEAP_AREA_END
#define MMAP_AREA_END		STACK_AREA_START
#define MMAP_AREA_SIZE		(MMAP_AREA_END - MMAP_AREA_START)

#else /* CONFIG_LIBPOSIX_MMAP */
/* When we don't use mmap, heap is the rest of the memory */
#define HEAP_AREA_END		STACK_AREA_START
#define HEAP_AREA_SIZE		(HEAP_AREA_END - HEAP_AREA_START)

#define MMAP_AREA_START		0x0
#define MMAP_AREA_END		0x0
#define MMAP_AREA_SIZE		0x0
#endif /* CONFIG_LIBPOSIX_MMAP */

#endif /* __UK_MEM_LAYOUT__ */

