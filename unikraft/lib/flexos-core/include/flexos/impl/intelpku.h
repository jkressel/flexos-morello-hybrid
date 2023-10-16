/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2020-2021, Pierre Olivier <pierre.olivier@manchester.ac.uk>
 *                          Hugo Lefeuvre <hugo.lefeuvre@manchester.ac.uk>
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

#ifndef FLEXOS_INTELPKU_H
#define FLEXOS_INTELPKU_H

#include <uk/config.h>
#include <uk/sections.h>
#include <stdint.h>

struct uk_alloc;

/* Shared allocator */
extern struct uk_alloc *flexos_shared_alloc;

/* flexos_comp0_alloc is just an alias for the standard, default allocator:
 * offers memory in domain zero.
 */
#define flexos_comp0_alloc _uk_alloc_head

/* The toolchain will insert allocator declarations here, e.g.:
 *
 * extern struct uk_alloc *flexos_comp1_alloc;
 *
 * for compartment 1.
 */
/* Dynamic allocator for compartment 1 */
extern struct uk_alloc *flexos_comp1_alloc;


typedef enum {
    PKU_RW,
    PKU_RO,
    PKU_NONE
} flexos_intelpku_perm;

/* Set the key associated with passed set of pages to key */
int flexos_intelpku_mem_set_key(void *page_boundary, uint64_t num_pages, uint8_t key);

/* Get the key associated with passed page */
int flexos_intelpku_mem_get_key(void *page_boundary);

/* Set permission for a given key and update PKRU accordingly */
int flexos_intelpku_set_perm(uint8_t key, flexos_intelpku_perm perm);

/* Low level C wrapper for RDPKRU: return the current protection key or
 * -ENOSPC if the CPU does not support PKU */
__attribute__((always_inline)) static inline uint32_t rdpkru(void)
{
	uint32_t res;
	asm volatile (  "xor %%ecx, %%ecx;"
			"rdpkru;"
			"movl %%eax, %0" : "=r"(res) :: "rax", "rdx", "ecx");

	return res;
}

/* Regarding the lfence here, see Spectre 1.1 paper, 'Speculative Buffer
 * Overflows: Attacks and Defenses' */
__attribute__((always_inline)) static inline void wrpkru(uint32_t val)
{
	/* FIXME FLEXOS: should we clobber "memory" here? */
	asm volatile (  "mov %0, %%eax;"
			"xor %%ecx, %%ecx;"
			"xor %%edx, %%edx;"
			"wrpkru;"
			"lfence"
			:: "r"(val) : "eax", "ecx", "edx");
}

/* The following Coccinelle rule is very useful to find gate calls
 * with more than 6 arguments.
 *
 * @@
 * expression list[n > 9] EL;
 * type T;
 * @@
 * + // invalid
 * flexos_intelpku_gate(EL)
 */

/* Nasty argument counting trick from
 * https://stackoverflow.com/questions/4421681 */
#define ELEVENTH_ARGUMENT(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, ...) a11
#define COUNT_ARGUMENTS(...) ELEVENTH_ARGUMENT(dummy, ## __VA_ARGS__, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

/* flexos_intelpku_gate(1, 0, printf, "hello\n")
 * -> execute printf("hello\n") is protection domain 0 */
/* FIXME FLEXOS HUGE HACK! Disable gates for interrupt handlers. This
 * is a huge security hole. DO NOT DO THAT IN PRACTICE. We only do it
 * for SOSP because we don't have time for a proper fix and it doesn't
 * impact performance.
 */
#define _eflexos_intelpku_gate(N, key_from, key_to, fname, ...)		\
	_flexos_intelpku_gate(N, key_from, key_to, fname, ## __VA_ARGS__)
#define flexos_intelpku_gate(key_from, key_to, fname, ...)		\
do {									\
	if (ukarch_read_sp() >= __INTRSTACK_START &&			\
	    ukarch_read_sp() <= __END) {				\
		fname(__VA_ARGS__);					\
	} else {							\
		_flexos_intelpku_gate_inst_in(key_from, key_to, #fname);\
		_eflexos_intelpku_gate(COUNT_ARGUMENTS(__VA_ARGS__),	\
			key_from, key_to, fname, ## __VA_ARGS__);	\
		_flexos_intelpku_gate_inst_out(key_from, key_to);	\
	}								\
} while (0)

#define _eflexos_intelpku_gate_r(N, key_from, key_to, retval, fname, ...)\
	_flexos_intelpku_gate_r(N, key_from, key_to, retval, fname, ## __VA_ARGS__)
/* second level of indirection is required to expand N */
#define flexos_intelpku_gate_r(key_from, key_to, retval, fname, ...)	\
do {									\
	if (ukarch_read_sp() >= __INTRSTACK_START &&			\
	    ukarch_read_sp() <= __END) {				\
		retval = fname(__VA_ARGS__);				\
	} else {							\
		_flexos_intelpku_gate_inst_in(key_from, key_to, #fname);\
		_eflexos_intelpku_gate_r(COUNT_ARGUMENTS(__VA_ARGS__),	\
			key_from, key_to, retval, fname, ## __VA_ARGS__);\
		_flexos_intelpku_gate_inst_out(key_from, key_to);	\
	}								\
} while (0)

/* TODO FLEXOS this does no harm, but it's code duplication.
 * Seems necessary to compile nginx.
 */
#ifndef flexos_nop_gate
#define flexos_nop_gate(key_from, key_to, func, ...) func(__VA_ARGS__)
#define flexos_nop_gate_r(key_from, key_to, ret, func, ...) ret = func(__VA_ARGS__)
#endif

/* TODO FLEXOS: sharing or not the stack should be decided on a
 * per-compartment basis, so this config option is certainly not the
 * right way to do it. */
#if !CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS
struct uk_thread_status_block {
	uint64_t sp;
	uint64_t bp;
};

extern struct uk_thread_status_block tsb_comp0[32];
/* __FLEXOS MARKER__: insert tsb extern decls here. */
#endif

/* Sanitize options a little bit more */

#if CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS && CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS
#error "Shared stacks and private stacks options are incompatible!"
#endif /* CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS && CONFIG_LIBFLEXOS_ENABLE_DSS */

#if CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS && CONFIG_LIBFLEXOS_ENABLE_DSS
#error "Shared stacks and DSS options are incompatible!"
#endif /* CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS && CONFIG_LIBFLEXOS_ENABLE_DSS */

#endif /* FLEXOS_INTELPKU_H */
