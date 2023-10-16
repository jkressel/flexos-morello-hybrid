/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2020-2021, Hugo Lefeuvre <hugo.lefeuvre@manchester.ac.uk>
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

#ifndef FLEXOS_INTELPKU_IMPL_H
#define FLEXOS_INTELPKU_IMPL_H

#include <flexos/impl/intelpku.h>
#include <uk/alloc.h>
#include <uk/plat/config.h> /* STACK_SIZE */
#include <uk/essentials.h> /* ALIGN_UP */
#include <uk/arch/lcpu.h>
#include <uk/assert.h> /* UK_CRASH */
#include <string.h> /* memcpy */
#include <uk/page.h> /* round_pgup() */

#define PKRU(key1)	(0x3fffffff & ~(1UL << ( key1 * 2))		\
				& ~(1UL << ((key1 * 2) + 1)))

static inline
int uk_thread_get_tid(void)
{
	unsigned long sp = ukarch_read_sp();
	return *((int *) round_pgup((unsigned long) ((sp & STACK_MASK_TOP) + 1)));
}

/* ==========================================================================
 * Implementation of PKU gate instrumentation
 * ========================================================================== */

/* Enable/Disable gate instrumentations.
 * - CONFIG_LIBFLEXOS_COUNT_GATE_EXECUTIONS: count number of gates
 *   executed.
 * - CONFIG_LIBFLEXOS_GATE_INTELPKU_DBG: gate sanity checks. */
#if CONFIG_LIBFLEXOS_COUNT_GATE_EXECUTIONS && CONFIG_LIBFLEXOS_GATE_INTELPKU_DBG
#error "The debug gate is incompatible with the gate execution counter!"
#elif CONFIG_LIBFLEXOS_COUNT_GATE_EXECUTIONS
extern volatile unsigned long flexos_intelpku_in_gate_counter;
extern volatile unsigned long flexos_intelpku_out_gate_counter;
#define _flexos_intelpku_gate_inst_in(f, t, fname) 			\
	__flexos_intelpku_gate_counter_in(f, t, fname)
#define _flexos_intelpku_gate_inst_out(f, t)				\
	__flexos_intelpku_gate_counter_out(f, t)
#elif CONFIG_LIBFLEXOS_GATE_INTELPKU_DBG
#define _flexos_intelpku_gate_inst_in(f, t, fname) 			\
	__flexos_intelpku_gate_dbg_in(f, t)
#define _flexos_intelpku_gate_inst_out(f, t)
#else /* No instrumentation */
#define _flexos_intelpku_gate_inst_in(f, t, fname)
#define _flexos_intelpku_gate_inst_out(f, t)
#endif

#if CONFIG_LIBFLEXOS_COUNT_GATE_EXECUTIONS
#define flexos_intelpku_print_in_counter()				\
do {									\
	printf("Number of 'in' gates executed: %lu\n",			\
		flexos_intelpku_in_gate_counter);			\
} while (0)

#define flexos_intelpku_print_out_counter()				\
do {									\
	printf("Number of 'out' gates executed: %lu\n",			\
		flexos_intelpku_out_gate_counter);			\
} while (0)

#define flexos_intelpku_reset_gate_counters()				\
do {									\
	flexos_intelpku_out_gate_counter = 0;				\
	flexos_intelpku_in_gate_counter = 0;				\
} while (0)

#define __flexos_intelpku_gate_counter_out(k_from, k_to)		\
do {									\
	flexos_intelpku_out_gate_counter++;				\
} while (0)

#define __flexos_intelpku_gate_counter_in(k_from, k_to, fname)	\
do {									\
	/* only temporary to enable access to printf */			\
	wrpkru(0x3ffffff0);						\
	printf("switch triggered by %s (%d -> %d)\n", fname,		\
				k_from, k_to);				\
									\
	flexos_intelpku_in_gate_counter++;				\
} while (0)
#endif /* CONFIG_LIBFLEXOS_COUNT_GATE_EXECUTIONS */

/* debug gate: this has a cost, don't use it for benchmarks */
#define __flexos_intelpku_gate_dbg_in(key_from, key_to) 	\
do { 									\
	/* sanity check: did we enter this code from an */		\
	/* unexpected domain? */					\
	uint32_t pkru = rdpkru();					\
									\
	if (pkru != PKRU(key_from))	{				\
		/* at this point we detected a fatal bug, so just */	\
		/* take full permissions and crash */			\
		wrpkru(0x0);						\
		UK_CRASH("ERROR IN GATE got %#010x, "			\
			 "expected %#010x", pkru, PKRU(key_from));	\
	}								\
} while (0)


/* ==========================================================================
 * Implementation of PKU gates
 * ========================================================================== */

#if CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS

#define __flexos_intelpku_gate_swpkru(key_from, key_to)			\
do {									\
	/* switch thread permissions, this includes anti-ROP checks */	\
	asm volatile (  "1:\n\t" /* define local label */		\
			"movq %0, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %0, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
			:: "i"(PKRU(key_to)) : "rax", "rcx", "rdx");	\
} while (0)

#define _flexos_intelpku_gate(N, key_from, key_to, fname, ...)		\
do {									\
	__flexos_intelpku_gate_swpkru(key_from, key_to);		\
	fname(__VA_ARGS__);						\
	__flexos_intelpku_gate_swpkru(key_to, key_from);		\
} while (0)

#define _flexos_intelpku_gate_r(N, key_from, key_to, retval, fname, ...)\
do {									\
	__flexos_intelpku_gate_swpkru(key_from, key_to);		\
	retval = fname(__VA_ARGS__);					\
	__flexos_intelpku_gate_swpkru(key_to, key_from);		\
} while (0)

#else

/* We can clobber %r11 here */
#define __ASM_BACKUP_TSB(tsb_comp)					\
	/* load tid into %r12 */					\
	"movq %%r15,%%r11\n\t"						\
	/* %r12 = tid * sizeof(struct uk_thread_status_block) */	\
	"shl $0x4,%%r11\n\t"						\
	/* %r12 = &tsb_compN[tid * sizeof(struct uk_thread_status_block)] */\
	"addq $" STRINGIFY(tsb_comp) ",%%r11\n\t"			\
	/* push tsb_compN[tid * sizeof(struct uk_thread_status_block)].sp */\
	"push (%%r11)\n\t"						\
	/* push tsb_compN[tid * sizeof(struct uk_thread_status_block)].bp */\
	"addq $0x8,%%r11\n\t"						\
	"push (%%r11)\n\t"

#define __ASM_UPDATE_TSB_TMP(tsb_comp)					\
	/* load tid into %r12 */					\
	"movq %%r15,%%r11\n\t"						\
	/* %r12 = tid * sizeof(struct uk_thread_status_block) */	\
	"shl $0x4,%%r11\n\t"						\
	/* %r12 = &tsb_compN[tid * sizeof(struct uk_thread_status_block)] */\
	"addq $" STRINGIFY(tsb_comp) ",%%r11\n\t"			\
	/* %rcx = &tsb_compN[tid * sizeof(struct uk_thread_status_block)] */\
	"lea (%%r11),%%rcx\n\t"						\
	/* tsb_compN[tid * sizeof(struct uk_thread_status_block)].sp = %rsp */\
	"movq %%rsp,(%%r11)\n\t"					\
	/* tsb_compN[tid * sizeof(struct uk_thread_status_block)].bp = %rbp */\
	"movq %%rbp,0x8(%%rcx)\n\t"

/* Do not clobber anything here */
#define __ASM_SWITCH_STACK(tsb_comp)					\
	/* load tid into %rsp */					\
	"movq %%r15,%%rsp\n\t"						\
	/* %rsp = tid * sizeof(struct uk_thread_status_block) */	\
	"shl $0x4,%%rsp\n\t"						\
	/* %rsp = &tsb_compN[tid * sizeof(struct uk_thread_status_block)] */\
	"addq $" STRINGIFY(tsb_comp) ",%%rsp\n\t"			\
	/* %rbp = &tsb_compN[tid * sizeof(struct uk_thread_status_block)] */\
	"movq %%rsp,%%rbp\n\t"						\
	/* %rsp = tsb_compN[tid * sizeof(struct uk_thread_status_block)].sp */\
	"movq (%%rsp),%%rsp\n\t"					\
	/* %rbp = tsb_compN[tid * sizeof(struct uk_thread_status_block)].bp */\
	"addq $0x8,%%rbp\n\t"						\
	"movq (%%rbp),%%rbp\n\t"

#define __ASM_ALIGN_AND_CALL(fname)					\
	/* align at 16 bytes */						\
	"andq $-16, %%rsp\n\t"						\
	/* call the actual function */					\
	"call " #fname "\n\t"						\

/* We can clobber %r12 here */
#define __ASM_RESTORE_TSB(tsb_comp)					\
	/* load tid into %r12 */					\
	"movq %%r15,%%r12\n\t"						\
	/* %r12 = tid * sizeof(struct uk_thread_status_block) */	\
	"shl $0x4,%%r12\n\t"						\
	/* %r12 = &tsb_compN[tid * sizeof(struct uk_thread_status_block)] */\
	"addq $" STRINGIFY(tsb_comp) ",%%r12\n\t"			\
	/* pop to tsb_compN[tid * sizeof(struct uk_thread_status_block)].bp */\
	"addq $0x8,%%r12\n\t"						\
	"pop (%%r12)\n\t"						\
	/* pop to tsb_compN[tid * sizeof(struct uk_thread_status_block)].sp */\
	"subq $0x8,%%r12\n\t"						\
	"pop (%%r12)\n\t"

/* TODO FLEXOS deduplicate this code */

#define __flexos_intelpku_gate0(key_from, key_to, fname)		\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* nothing to do: no parameters */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			/* nothing to do: no parameters */		\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			/* nothing to do: no return value */		\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %2, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %2, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			/* nothing to do: no return value */		\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
} while (0)

#define __flexos_intelpku_gate0_r(key_from, key_to, retval, fname)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
	register uint64_t _ret asm("r11");				\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* nothing to do: no parameters */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %4, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %4, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			/* nothing to do: no parameters */		\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			"movq %%rax, %%rsi\n\t"				\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			"movq %%rsi, %%r11\n\t"				\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=r" (_ret), /* always %%r11 */		\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
									\
	/* this will be optimized by the compiler */			\
	if (sizeof(retval) == 1) /* 8 bit */				\
		asm volatile (						\
			"mov %%r11b, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	if (sizeof(retval) == 2) /* 16 bit */				\
		asm volatile (						\
			"mov %%r11w, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 4) /* 32 bit */			\
		asm volatile (						\
			"mov %%r11d, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 8) /* 64 bit */			\
		asm volatile (						\
			"mov %%r11, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
} while (0)

/* This is living hell. */

#define __flexos_intelpku_gate1(key_from, key_to, fname, arg1)		\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 via input constraints */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			/* nothing to do: 1 already final */		\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			/* nothing to do: no return value */		\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %2, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %2, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			/* nothing to do: no return value */		\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))), /* ask for %rdi */	\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
} while (0)

#define __flexos_intelpku_gate1_r(key_from, key_to, retval, fname, arg1)\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
	register uint64_t _ret asm("r11");				\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 via input constraints */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %4, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %4, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			/* nothing to do: 1 already final */		\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			"movq %%rax, %%rsi\n\t"				\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			"movq %%rsi, %%r11\n\t"				\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=r" (_ret), /* always %%r11 */		\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
									\
	/* this will be optimized by the compiler */			\
	if (sizeof(retval) == 1) /* 8 bit */				\
		asm volatile (						\
			"mov %%r11b, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	if (sizeof(retval) == 2) /* 16 bit */				\
		asm volatile (						\
			"mov %%r11w, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 4) /* 32 bit */			\
		asm volatile (						\
			"mov %%r11d, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 8) /* 64 bit */			\
		asm volatile (						\
			"mov %%r11, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
} while (0)

#define __flexos_intelpku_gate2(key_from, key_to, fname, arg1, arg2)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: args 1 & 2 via input constraints */	\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			/* nothing to do: 1 & 2 already final */	\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			/* nothing to do: no return value */		\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %2, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %2, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			/* nothing to do: no return value */		\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
} while (0)

#define __flexos_intelpku_gate2_r(key_from, key_to, retval, fname,	\
							arg1, arg2)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
	register uint64_t _ret asm("r11");				\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: args 1 & 2 via input constraints */	\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %4, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %4, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			/* nothing to do: 1 & 2 already final */	\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			"movq %%rax, %%rsi\n\t"				\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			"movq %%rsi, %%r11\n\t"				\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=r" (_ret), /* always %%r11 */		\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
									\
	/* this will be optimized by the compiler */			\
	if (sizeof(retval) == 1) /* 8 bit */				\
		asm volatile (						\
			"mov %%r11b, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	if (sizeof(retval) == 2) /* 16 bit */				\
		asm volatile (						\
			"mov %%r11w, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 4) /* 32 bit */			\
		asm volatile (						\
			"mov %%r11d, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 8) /* 64 bit */			\
		asm volatile (						\
			"mov %%r11, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
} while (0)

#define __flexos_intelpku_gate3(key_from, key_to, fname, arg1, arg2,	\
							 arg3)		\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
									\
	/* No inline asm input constraints for r12, so do */		\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 in r12 for now */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			/* nothing to do: no return value */		\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %2, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %2, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			/* nothing to do: no return value */		\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
} while (0)

#define __flexos_intelpku_gate3_r(key_from, key_to, retval, fname,	\
					arg1, arg2, arg3)		\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
	register uint64_t _ret asm("r11");				\
									\
	/* No inline asm input constraints for r12, so do */		\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 in r12 for now */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %4, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %4, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			"movq %%rax, %%rsi\n\t"				\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			"movq %%rsi, %%r11\n\t"				\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=r" (_ret), /* always %%r11 */		\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
									\
	/* this will be optimized by the compiler */			\
	if (sizeof(retval) == 1) /* 8 bit */				\
		asm volatile (						\
			"mov %%r11b, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	if (sizeof(retval) == 2) /* 16 bit */				\
		asm volatile (						\
			"mov %%r11w, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 4) /* 32 bit */			\
		asm volatile (						\
			"mov %%r11d, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 8) /* 64 bit */			\
		asm volatile (						\
			"mov %%r11, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
} while (0)

#define __flexos_intelpku_gate4(key_from, key_to, fname, arg1, arg2,	\
							 arg3, arg4)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
									\
	/* No inline asm input constraints for r12-13, so do */		\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
	register uint64_t _arg4 asm("r13") = (uint64_t) arg4;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
			"push %%r13\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 & 4 in r12-13 for now */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
			"movq %%r13, %%rcx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			/* nothing to do: no return value */		\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %2, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %2, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r13\n\t"					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			/* nothing to do: no return value */		\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(_arg4),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
} while (0)

#define __flexos_intelpku_gate4_r(key_from, key_to, retval, fname,	\
						arg1, arg2, arg3, arg4)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
	register uint64_t _ret asm("r11");				\
									\
	/* No inline asm input constraints for r12-13, so do */		\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
	register uint64_t _arg4 asm("r13") = (uint64_t) arg4;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
			"push %%r13\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 & 4 in r12-13 for now */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %4, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %4, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
			"movq %%r13, %%rcx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			"movq %%rax, %%rsi\n\t"				\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r13\n\t"					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			"movq %%rsi, %%r11\n\t"				\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=r" (_ret), /* always %%r11 */		\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(_arg4),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
									\
	/* this will be optimized by the compiler */			\
	if (sizeof(retval) == 1) /* 8 bit */				\
		asm volatile (						\
			"mov %%r11b, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	if (sizeof(retval) == 2) /* 16 bit */				\
		asm volatile (						\
			"mov %%r11w, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 4) /* 32 bit */			\
		asm volatile (						\
			"mov %%r11d, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 8) /* 64 bit */			\
		asm volatile (						\
			"mov %%r11, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
} while (0)

#define __flexos_intelpku_gate5(key_from, key_to, fname, arg1, arg2,	\
							 arg3, arg4,	\
							 arg5)		\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
									\
	/* No inline asm input constraints for r8,12-13, so do */	\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
	register uint64_t _arg4 asm("r13") = (uint64_t) arg4;		\
	register uint64_t _arg5 asm("r8") = (uint64_t) arg5;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
			"push %%r13\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 & 4 in r12-13 for now */		\
			/* note: arg 5 via asm("r8") */			\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
			"movq %%r13, %%rcx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			/* nothing to do: no return value */		\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %2, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %2, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r13\n\t"					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			/* nothing to do: no return value */		\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(_arg4),					\
			  "r"(_arg5),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
} while (0)

#define __flexos_intelpku_gate5_r(key_from, key_to, retval, fname,	\
					arg1, arg2, arg3, arg4,	arg5)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
	register uint64_t _ret asm("r11");				\
									\
	/* No inline asm input constraints for r8,12-13, so do */	\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
	register uint64_t _arg4 asm("r13") = (uint64_t) arg4;		\
	register uint64_t _arg5 asm("r8") = (uint64_t) arg5;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
			"push %%r13\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 & 4 in r12-13 for now */		\
			/* note: arg 5 via asm("r8") */			\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %4, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %4, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
			"movq %%r13, %%rcx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			"movq %%rax, %%rsi\n\t"				\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r13\n\t"					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			"movq %%rsi, %%r11\n\t"				\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=r" (_ret), /* always %%r11 */		\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(_arg4),					\
			  "r"(_arg5),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
									\
	/* this will be optimized by the compiler */			\
	if (sizeof(retval) == 1) /* 8 bit */				\
		asm volatile (						\
			"mov %%r11b, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	if (sizeof(retval) == 2) /* 16 bit */				\
		asm volatile (						\
			"mov %%r11w, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 4) /* 32 bit */			\
		asm volatile (						\
			"mov %%r11d, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 8) /* 64 bit */			\
		asm volatile (						\
			"mov %%r11, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
} while (0)

#define __flexos_intelpku_gate6(key_from, key_to, fname, arg1, arg2,	\
							 arg3, arg4,	\
							 arg5, arg6)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
									\
	/* No inline asm input constraints for r8-9,12-13, so do */	\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
	register uint64_t _arg4 asm("r13") = (uint64_t) arg4;		\
	register uint64_t _arg5 asm("r8") = (uint64_t) arg5;		\
	register uint64_t _arg6 asm("r9") = (uint64_t) arg6;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
			"push %%r13\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 & 4 in r12-13 for now */		\
			/* note: arg 5 & 6 via asm("r8/9") */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
			"movq %%r13, %%rcx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			/* nothing to do: no return value */		\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %2, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %2, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r13\n\t"					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			/* nothing to do: no return value */		\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(_arg4),					\
			  "r"(_arg5),					\
			  "r"(_arg6),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
} while (0)

#define __flexos_intelpku_gate6_r(key_from, key_to, retval, fname,	\
				arg1, arg2, arg3, arg4,	arg5, arg6)	\
do {									\
	/* we have to call this with a valid/accessible stack,	*/	\
	/* so do it before switching thread permissions. Note	*/	\
	/* that the stack won't be accessible after switching	*/	\
	/* permissions, so we HAVE to store this in a register.	*/	\
	register uint32_t tid asm("r15") = uk_thread_get_tid();		\
	register uint64_t _ret asm("r11");				\
									\
	/* No inline asm input constraints for r8-9,12-13, so do */	\
	/* it this way */						\
	register uint64_t _arg3 asm("r12") = (uint64_t) arg3;		\
	register uint64_t _arg4 asm("r13") = (uint64_t) arg4;		\
	register uint64_t _arg5 asm("r8") = (uint64_t) arg5;		\
	register uint64_t _arg6 asm("r9") = (uint64_t) arg6;		\
									\
	asm volatile (							\
	/* save remaining parameter registers			*/	\
	/* TODO do we actually HAVE to do this from the		*/	\
	/* perspective of the C calling convention?		*/	\
			"push %%rsi\n\t"				\
			"push %%rdi\n\t"				\
			"push %%r8\n\t"					\
			"push %%r9\n\t"					\
	/* save caller-saved registers (r10-11). */			\
			"push %%r10\n\t"				\
			"push %%r11\n\t"				\
	/* protecting registers? save callee-saved registers	*/	\
	/* and zero them out (r12-15).				*/	\
			/* TODO */					\
			"push %%r12\n\t"				\
			"push %%r13\n\t"				\
	/* backup source domain's stack/frame pointers		*/	\
			__ASM_BACKUP_TSB(tsb_comp ## key_from)		\
			__ASM_UPDATE_TSB_TMP(tsb_comp ## key_from)	\
	/* put parameters in registers				*/	\
			/* note: arg 1 & 2 via input constraints */	\
			/* note: arg 3 & 4 in r12-13 for now */		\
			/* note: arg 5 & 6 via asm("r8/9") */		\
	/* switch thread permissions 				*/	\
		"1:\n\t" /* define local label */			\
			"movq %4, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %4, %%rax\n\t"				\
			"jne 1b\n\t" /* ROP detected, re-do it */	\
	/* put parameters in final registers			*/	\
			"movq %%r12, %%rdx\n\t"				\
			"movq %%r13, %%rcx\n\t"				\
	/* we're ready, switch stack */					\
			__ASM_SWITCH_STACK(tsb_comp ## key_to)		\
			__ASM_ALIGN_AND_CALL(fname)			\
	/* backup return value in rsi */				\
			"movq %%rax, %%rsi\n\t"				\
	/* switch back thread permissions */				\
		"2:\n\t" /* define local label */			\
			"movq %3, %%rax\n\t"				\
			"xor %%rcx, %%rcx\n\t"				\
			"xor %%rdx, %%rdx\n\t"				\
			"wrpkru\n\t"					\
			"lfence\n\t" /* TODO necessary? */		\
			"cmpq %3, %%rax\n\t"				\
			"jne 2b\n\t" /* ROP detected, re-do it */	\
	/* switch back the stack				*/	\
			__ASM_SWITCH_STACK(tsb_comp ## key_from)	\
			__ASM_RESTORE_TSB(tsb_comp ## key_from)		\
	/* protecting registers? restore callee-saved registers	*/	\
			/* TODO */					\
			"pop %%r13\n\t"					\
			"pop %%r12\n\t"					\
	/* restore caller-saved registers			*/	\
			"pop %%r11\n\t"					\
			"pop %%r10\n\t"					\
	/* restore parameter registers				*/	\
			"pop %%r9\n\t"					\
	/* save return value from rsi (into r11) */			\
			"movq %%rsi, %%r11\n\t"				\
			"pop %%r8\n\t"					\
			"pop %%rdi\n\t"					\
			"pop %%rsi\n\t"					\
									\
			: /* output */					\
			  "=r" (_ret), /* always %%r11 */		\
			  "=m" (tsb_comp ## key_from),			\
			  "=m" (tsb_comp ## key_to)			\
			: /* input */					\
			  "i"(PKRU(key_from)),				\
			  "i"(PKRU(key_to)),				\
			  "D"((uint64_t)((arg1))),			\
			  "S"((uint64_t)((arg2))),			\
			  "r"(_arg3),					\
			  "r"(_arg4),					\
			  "r"(_arg5),					\
			  "r"(_arg6),					\
			  "r"(tid),					\
			  "i"(fname)					\
			: /* clobbers */				\
			  "rax", "rcx", "rdx",				\
			  "memory" /* TODO should we clobber memory? */	\
	);								\
									\
	/* this will be optimized by the compiler */			\
	if (sizeof(retval) == 1) /* 8 bit */				\
		asm volatile (						\
			"mov %%r11b, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	if (sizeof(retval) == 2) /* 16 bit */				\
		asm volatile (						\
			"mov %%r11w, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 4) /* 32 bit */			\
		asm volatile (						\
			"mov %%r11d, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
	else if (sizeof(retval) == 8) /* 64 bit */			\
		asm volatile (						\
			"mov %%r11, %0\n\t"				\
			: /* output */					\
			  "=m" (retval)					\
			: /* input */					\
			  "r"(_ret)					\
		);							\
} while (0)

/* Multiplex depending on the number of arguments.
 * TODO support more than 6 parameters (passed on the stack then...)
 */

#define _flexos_intelpku_gate(N, key_from, key_to, fname, ...)		\
do {									\
	UK_CTASSERT(N <= 6);						\
	__flexos_intelpku_gate ## N (key_from, key_to, fname __VA_OPT__(,) __VA_ARGS__); \
} while (0)

#define _flexos_intelpku_gate_r(N, key_from, key_to, retval, fname, ...)\
do {									\
	UK_CTASSERT(N <= 6);						\
	__flexos_intelpku_gate ## N ## _r (key_from, key_to, retval, fname __VA_OPT__(,) __VA_ARGS__); \
} while (0)

#endif
#endif /* FLEXOS_INTELPKU_IMPL_H */
