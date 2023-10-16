/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Santiago Pagani <santiagopagani@gmail.com>
 * 			John A. Kressel <jkressel.apps@gmail.com>
 *
 * Copyright (c) 2020, NEC Laboratories Europe GmbH, NEC Corporation.
 *                     All rights reserved.
 * Copyright (c) 2022, John A. Kressel <jkressel.apps@gmail.com>
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
 */

#include <stdlib.h>
#include <uk/plat/irq.h>
#include <uk/print.h>
#include <uk/essentials.h>
#include <morello/irq.h>
#include <morello/time.h>
#include <morello/morello_info.h>
#include <arm/time.h>

static irq_handler_func_t irq_handlers[IRQS_MAX];

struct __a64regs exception_regs_a64;

int ukplat_irq_register(unsigned long irq, irq_handler_func_t func, void *arg __unused)
{
	switch (irq) {
		case IRQ_ID_ARM_GENERIC_TIMER:
			break;
		default:
			// Unsupported IRQ
			uk_pr_crit("ukplat_irq_register: Unsupported IRQ\n");
			return -1;
	}

	irq_handlers[irq] = func;
	return 0;
}

void ukplat_morello_print_exception_reason(uint32_t esr) {
	switch (esr & 0x3F) {
		case ADDR_SZ_FLT_L0:
			printf("Address size fault, level 0 of translation or translation table base register\n");
			break;
		case ADDR_SZ_FLT_L1:
			printf("Address size fault, level 1\n");
			break;
		case ADDR_SZ_FLT_L2:
			printf("Address size fault, level 2\n");
			break;
		case ADDR_SZ_FLT_L3:
			printf("Address size fault, level 3\n");
			break;
		case TRANS_FLT_L0:
			printf("Translation fault, level 0\n");
			break;
		case TRANS_FLT_L1:
			printf("Translation fault, level 1\n");
			break;
		case TRANS_FLT_L2:
			printf("Translation fault, level 2\n");
			break;
		case TRANS_FLT_L3:
			printf("Translation fault, level 3\n");
			break;
		case ACS_FLG_FLT_L1:
			printf("Access flag fault, level 1\n");
			break;
		case ACS_FLG_FLT_L2:
			printf("Access flag fault, level 2\n");
			break;
		case ACS_FLG_FLT_L3:
			printf("Access flag fault, level 3\n");
			break;
		case PERM_FLT_L1:
			printf("Permission fault, level 1\n");
			break;
		case PERM_FLT_L2:
			printf("Permission fault, level 2\n");
			break;
		case PERM_FLT_L3:
			printf("Permission fault, level 3\n");
			break;
		case SYNC_EXT_ABRT_NOT_TTW:
			printf("Synchronous External abort, not on translation table walk\n");
			break;
		case SYNC_EXT_ABRT_TTW_L0:
			printf("Synchronous External abort, on translation table walk, level 0\n");
			break;
		case SYNC_EXT_ABRT_TTW_L1:
			printf("Synchronous External abort, on translation table walk, level 1\n");
			break;
		case SYNC_EXT_ABRT_TTW_L2:
			printf("Synchronous External abort, on translation table walk, level 2\n");
			break;
		case SYNC_EXT_ABRT_TTW_L3:
			printf("Synchronous External abort, on translation table walk, level 3\n");
			break;
		case SYNC_PAR_ECC_ERR_MEM_ACC_NOT_TTW:
			printf("Synchronous parity or ECC error on memory access, not on translation table walk\n");
			break;
		case SYNC_PAR_ECC_ERR_MEM_ACC_TTW_L0:
			printf("Synchronous parity or ECC error on memory access on translation table walk, level 0\n");
			break;
		case SYNC_PAR_ECC_ERR_MEM_ACC_TTW_L1:
			printf("Synchronous parity or ECC error on memory access on translation table walk, level 1\n");
			break;
		case SYNC_PAR_ECC_ERR_MEM_ACC_TTW_L2:
			printf("Synchronous parity or ECC error on memory access on translation table walk, level 2\n");
			break;
		case SYNC_PAR_ECC_ERR_MEM_ACC_TTW_L3:
			printf("Synchronous parity or ECC error on memory access on translation table walk, level 3\n");
			break;
		case CAP_TAG_FLT:
			printf("Capability tag fault\n");
			break;
		case CAP_SEALED_FLT:
			printf("Capability sealed fault\n");
			break;
		case CAP_BOUND_FLT:
			printf("Capability bound fault\n");
			break;
		case CAP_PERM_FLT:
			printf("Capability permission fault\n");
			break;
		case PG_TBL_LC_SC:
			printf("Page table LC or SC permission violation fault\n");
			break;
		case TLB_CONFLICT_ABRT:
			printf("TLB conflict abort\n");
			break;
		case UNSUPPORTED_ATOMIC_HARDWARE_UPDATE_FLT:
			printf("Unsupported atomic hardware update fault, if the implementation includes xARMv8.1-TTHM\n");
			break;
		default:
			printf("Exception type not handled here, I suggest you take a look at the ESR bits and then consult the ISA Manual.");
	}
}

int ukplat_irq_init(struct uk_alloc *a __unused)
{
	for (unsigned int i = 0; i < IRQS_MAX; i++) {
		irq_handlers[i] = NULL;
	}
	// *DISABLE_BASIC_IRQS = 0xFFFFFFFF;
	// *DISABLE_IRQS_1 = 0xFFFFFFFF;
	// *DISABLE_IRQS_2 = 0xFFFFFFFF;
	disable_irq();
	irq_vector_init();
	enable_irq();
	return 0;
}

void show_invalid_entry_message(int type)
{
	printf("IRQ: %d\n", type);
}

void dump_registers(int type, struct __a64regs *regs) {
	printf("IRQ: %d\n", type);
	printf("c0 lo: 0x%x hi: 0x%x\n", regs->x0, regs->x0_hi);
	printf("c1 lo: 0x%x hi: 0x%x\n", regs->x1, regs->x1_hi);
	printf("c2 lo: 0x%x hi: 0x%x\n", regs->x2, regs->x2_hi);
	printf("c3 lo: 0x%x hi: 0x%x\n", regs->x3, regs->x3_hi);
	printf("c4 lo: 0x%x hi: 0x%x\n", regs->x4, regs->x4_hi);
	printf("c5 lo: 0x%x hi: 0x%x\n", regs->x5, regs->x5_hi);
	printf("c6 lo: 0x%x hi: 0x%x\n", regs->x6, regs->x6_hi);
	printf("c7 lo: 0x%x hi: 0x%x\n", regs->x7, regs->x7_hi);
	printf("c8 lo: 0x%x hi: 0x%x\n", regs->x8, regs->x8_hi);
	printf("c9 lo: 0x%x hi: 0x%x\n", regs->x9, regs->x9_hi);
	printf("c10 lo: 0x%x hi: 0x%x\n", regs->x10, regs->x10_hi);
	printf("c11 lo: 0x%x hi: 0x%x\n", regs->x11, regs->x11_hi);
	printf("c12 lo: 0x%x hi: 0x%x\n", regs->x12, regs->x12_hi);
	printf("c13 lo: 0x%x hi: 0x%x\n", regs->x13, regs->x13_hi);
	printf("c14 lo: 0x%x hi: 0x%x\n", regs->x14, regs->x14_hi);
	printf("c15 lo: 0x%x hi: 0x%x\n", regs->x15, regs->x15_hi);
	printf("c16 lo: 0x%x hi: 0x%x\n", regs->x16, regs->x16_hi);
	printf("c17 lo: 0x%x hi: 0x%x\n", regs->x17, regs->x17_hi);
	printf("c18 lo: 0x%x hi: 0x%x\n", regs->x18, regs->x18_hi);
	printf("c19 lo: 0x%x hi: 0x%x\n", regs->x19, regs->x19_hi);
	printf("c20 lo: 0x%x hi: 0x%x\n", regs->x20, regs->x20_hi);
	printf("c21 lo: 0x%x hi: 0x%x\n", regs->x21, regs->x21_hi);
	printf("c22 lo: 0x%x hi: 0x%x\n", regs->x22, regs->x22_hi);
	printf("c23 lo: 0x%x hi: 0x%x\n", regs->x23, regs->x23_hi);
	printf("c24 lo: 0x%x hi: 0x%x\n", regs->x24, regs->x24_hi);
	printf("c25 lo: 0x%x hi: 0x%x\n", regs->x25, regs->x25_hi);
	printf("c26 lo: 0x%x hi: 0x%x\n", regs->x26, regs->x26_hi);
	printf("c27 lo: 0x%x hi: 0x%x\n", regs->x27, regs->x27_hi);
	printf("c28 lo: 0x%x hi: 0x%x\n", regs->x28, regs->x28_hi);
	printf("fp lo: 0x%x hi: 0x%x\n", regs->x29, regs->x29_hi);
	printf("lr lo: 0x%x hi: 0x%x\n", regs->x30, regs->x30_hi);
	printf("sp: 0x%x\n", regs->x31);
	printf("esr_el1: 0x%x\n", regs->esr_el1);
	printf("elr_el1: 0x%x\n", regs->elr_el1);
	printf("spsr_el1: 0x%x\n", regs->spsr_el1);
	printf("cctlr_el1: 0x%x\n", regs->cctlr);
	printf("cpacr_el1: 0x%x\n", regs->cpacr_el1);
	printf("far_el1: 0x%x\n", regs->far_el1);
	ukplat_morello_print_exception_reason(regs->esr_el1);
}

void show_invalid_entry_message_el1_sync(uint64_t esr_el, uint64_t far_el)
{
	uk_pr_debug("ESR_EL1: %lx, FAR_EL1: %lx, SCTLR_EL1:%lx\n", esr_el, far_el, get_sctlr_el1());
}

void ukplat_irq_handle(void)
{

	if ((get_el0(cntv_ctl) & GT_TIMER_IRQ_STATUS) && irq_handlers[IRQ_ID_ARM_GENERIC_TIMER]) {
		irq_handlers[IRQ_ID_ARM_GENERIC_TIMER](NULL);
		return;
	}

	uk_pr_crit("ukplat_irq_handle: Unhandled IRQ\n");
	while(1);
}
