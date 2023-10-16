/*
 * MIT License
 *
 * Copyright (c) 2018, Sergey Matyukevich
 *           (c) 2020, Santiago Pagani <santiagopagani@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 */

#include <morello/entry.h>

	.macro handle_invalid_entry type
	kernel_entry
	stp x1, x0, [sp, #-16]
	cfhi x1, c1
	cfhi x0, c0
	stp x1, x0, [sp, #-32]
	ldr x0, =(exception_regs_a64)
	ldr x1, [sp, #-8]
	str x1, [x0, #48]
	ldr x1, [sp, #-24]
	str x1, [x0, #56]
	ldr x1, [sp, #-16]
	str x1, [x0, #64]
	ldr x1, [sp, #-32]
	str x1, [x0, #72]
	cfhi x1, c2
	str x1, [x0, #88]
	str x2, [x0, #80]
	cfhi x1, c3
	str x1, [x0, #104]
	str x3, [x0, #96]
	cfhi x1, c4
	str x1, [x0, #120]
	str x4, [x0, #112]
	cfhi x1, c5
	str x1, [x0, #136]
	str x5, [x0, #128]
	cfhi x1, c6
	str x1, [x0, #152]
	str x6, [x0, #144]
	cfhi x1, c7
	str x1, [x0, #168]
	str x7, [x0, #160]
	cfhi x1, c8
	str x1, [x0, #184]
	str x8, [x0, #176]
	cfhi x1, c9
	str x1, [x0, #200]
	str x9, [x0, #192]
	cfhi x1, c10
	str x1, [x0, #216]
	str x10, [x0, #208]
	cfhi x1, c11
	str x1, [x0, #232]
	str x11, [x0, #216]
	cfhi x1, c12
	str x1, [x0, #248]
	str x12, [x0, #240]
	cfhi x1, c13
	str x1, [x0, #264]
	str x13, [x0, #256]
	cfhi x1, c14
	str x1, [x0, #280]
	str x14, [x0, #272]
	cfhi x1, c15
	str x1, [x0, #296]
	str x15, [x0, #288]
	cfhi x1, c16
	str x1, [x0, #312]
	str x16, [x0, #304]
	cfhi x1, c17
	str x1, [x0, #328]
	str x17, [x0, #320]
	cfhi x1, c18
	str x1, [x0, #344]
	str x18, [x0, #336]
	cfhi x1, c19
	str x1, [x0, #360]
	str x19, [x0, #352]
	cfhi x1, c20
	str x1, [x0, #376]
	str x20, [x0, #368]
	cfhi x1, c21
	str x1, [x0, #392]
	str x21, [x0, #384]
	cfhi x1, c22
	str x1, [x0, #408]
	str x22, [x0, #400]
	cfhi x1, c23
	str x1, [x0, #424]
	str x23, [x0, #416]
	cfhi x1, c24
	str x1, [x0, #440]
	str x24, [x0, #432]
	cfhi x1, c25
	str x1, [x0, #456]
	str x25, [x0, #448]
	cfhi x1, c26
	str x1, [x0, #472]
	str x26, [x0, #464]
	cfhi x1, c27
	str x1, [x0, #488]
	str x27, [x0, #480]
	cfhi x1, c28
	str x1, [x0, #504]
	str x28, [x0, #496]
	cfhi x1, c29
	str x1, [x0, #520]
	str x29, [x0, #512]
	cfhi x1, c30
	str x1, [x0, #536]
	str x30, [x0, #528]
	mov x1, sp
	str x1, [x0, #544]
	mrs x1, esr_el1
	str x1, [x0]
	mrs x1, elr_el1
	str x1, [x0, #8]
	mrs x1, spsr_el1
	str x1, [x0, #16]
	mrs x1, cctlr_el1
	str x1, [x0, #24]
	mrs x1, cpacr_el1
	str x1, [x0, #32]
	mrs x1, far_el1
	str x1, [x0, #40]
	mov x1, x0
	mov	x0, #\type
	bl	dump_registers
	wfi
	msr             SPSel, #0
	mov x17, sp
	msr             SPSel, #1
	nop
	nop
	nop
	nop
	mov sp, x17
#ifdef MORELLO_EXCEPTION_SWITCH_DDC
	mrs x17, spsr_el1
	eor x17, x17, #1
	msr spsr_el1, x17
	isb
	eret
#endif
//	kernel_exit
	
	.endm

	.macro	ventry	label
	.align	7
	b	\label
	.endm

	.macro	kernel_entry
	sub	sp, sp, #S_FRAME_SIZE
	stp	c0, c1, [sp, #32 * 0]
	stp	c2, c3, [sp, #32 * 1]
	stp	c4, c5, [sp, #32 * 2]
	stp	c6, c7, [sp, #32 * 3]
	stp	c8, c9, [sp, #32 * 4]
	stp	c10, c11, [sp, #32 * 5]
	stp	c12, c13, [sp, #32 * 6]
	stp	c14, c15, [sp, #32 * 7]
	stp	c16, c17, [sp, #32 * 8]
	stp	c18, c19, [sp, #32 * 9]
	stp	c20, c21, [sp, #32 * 10]
	stp	c22, c23, [sp, #32 * 11]
	stp	c24, c25, [sp, #32 * 12]
	stp	c26, c27, [sp, #32 * 13]
	stp	c28, c29, [sp, #32 * 14]
	str	c30, [sp, #32 * 15] 
	.endm

	.macro	kernel_exit
	ldp	c0, c1, [sp, #32 * 0]
	ldp	c2, c3, [sp, #32 * 1]
	ldp	c4, c5, [sp, #32 * 2]
	ldp	c6, c7, [sp, #32 * 3]
	ldp	c8, c9, [sp, #32 * 4]
	ldp	c10, c11, [sp, #32 * 5]
	ldp	c12, c13, [sp, #32 * 6]
	ldp	c14, c15, [sp, #32 * 7]
	ldp	c16, c17, [sp, #32 * 8]
	ldp	c18, c19, [sp, #32 * 9]
	ldp	c20, c21, [sp, #32 * 10]
	ldp	c22, c23, [sp, #32 * 11]
	ldp	c24, c25, [sp, #32 * 12]
	ldp	c26, c27, [sp, #32 * 13]
	ldp	c28, c29, [sp, #32 * 14]
	ldr	c30, [sp, #32 * 15] 
	add	sp, sp, #S_FRAME_SIZE		
	eret
	.endm


/*
 * Exception vectors.
 */
.align	11
.globl vectors_el3
vectors_el3:
	ventry	sync_invalid_el3t			// Synchronous EL3t
	ventry	irq_invalid_el3t			// IRQ EL3t
	ventry	fiq_invalid_el3t			// FIQ EL3t
	ventry	error_invalid_el3t			// Error EL3t

	ventry	sync_invalid_el3h			// Synchronous EL3h
	ventry	irq_invalid_el3h			// IRQ EL3h
	ventry	fiq_invalid_el3h			// FIQ EL3h
	ventry	error_invalid_el3h			// Error EL3h

	ventry	sync_invalid_el2_64			// Synchronous 64-bit EL2
	ventry	irq_invalid_el2_64			// IRQ 64-bit EL2
	ventry	fiq_invalid_el2_64			// FIQ 64-bit EL2
	ventry	error_invalid_el2_64		// Error 64-bit EL2

	ventry	sync_invalid_el2_32			// Synchronous 32-bit EL2
	ventry	irq_invalid_el2_32			// IRQ 32-bit EL2
	ventry	fiq_invalid_el2_32			// FIQ 32-bit EL2
	ventry	error_invalid_el2_32		// Error 32-bit EL2

.align	11
.globl vectors_el2
vectors_el2:
	ventry	sync_invalid_el2t			// Synchronous EL2t
	ventry	irq_invalid_el2t			// IRQ EL2t
	ventry	fiq_invalid_el2t			// FIQ EL2t
	ventry	error_invalid_el2t			// Error EL2t

	ventry	sync_invalid_el2h			// Synchronous EL2h
	ventry	irq_invalid_el2h			// IRQ EL2h
	ventry	fiq_invalid_el2h			// FIQ EL2h
	ventry	error_invalid_el2h			// Error EL2h

	ventry	sync_invalid_el1_64			// Synchronous 64-bit EL1
	ventry	irq_invalid_el1_64			// IRQ 64-bit EL1
	ventry	fiq_invalid_el1_64			// FIQ 64-bit EL1
	ventry	error_invalid_el1_64		// Error 64-bit EL1

	ventry	sync_invalid_el1_32			// Synchronous 32-bit EL1
	ventry	irq_invalid_el1_32			// IRQ 32-bit EL1
	ventry	fiq_invalid_el1_32			// FIQ 32-bit EL1
	ventry	error_invalid_el1_32		// Error 32-bit EL1

.align	11
.globl vectors_el1 
vectors_el1:
	ventry	sync_invalid_el1t			// Synchronous EL1t
	ventry	irq_invalid_el1t			// IRQ EL1t
	ventry	fiq_invalid_el1t			// FIQ EL1t
	ventry	error_invalid_el1t			// Error EL1t

	ventry	el1_sync					// Synchronous EL1h
	ventry	el1_irq						// IRQ EL1h
	ventry	fiq_invalid_el1h			// FIQ EL1h
	ventry	error_invalid_el1h			// Error EL1h

	ventry	sync_invalid_el0_64			// Synchronous 64-bit EL0
	ventry	irq_invalid_el0_64			// IRQ 64-bit EL0
	ventry	fiq_invalid_el0_64			// FIQ 64-bit EL0
	ventry	error_invalid_el0_64		// Error 64-bit EL0

	ventry	sync_invalid_el0_32			// Synchronous 32-bit EL0
	ventry	irq_invalid_el0_32			// IRQ 32-bit EL0
	ventry	fiq_invalid_el0_32			// FIQ 32-bit EL0
	ventry	error_invalid_el0_32		// Error 32-bit EL0

sync_invalid_el3t:
	handle_invalid_entry  SYNC_INVALID_EL3t

irq_invalid_el3t:
	handle_invalid_entry  IRQ_INVALID_EL3t

fiq_invalid_el3t:
	handle_invalid_entry  FIQ_INVALID_EL3t

error_invalid_el3t:
	handle_invalid_entry  ERROR_INVALID_EL3t

sync_invalid_el3h:
	handle_invalid_entry  SYNC_INVALID_EL3h

irq_invalid_el3h:
	handle_invalid_entry  IRQ_INVALID_EL3h

fiq_invalid_el3h:
	handle_invalid_entry  FIQ_INVALID_EL3h

error_invalid_el3h:
	handle_invalid_entry  ERROR_INVALID_EL3h

sync_invalid_el2_64:
	handle_invalid_entry  SYNC_INVALID_EL2_64

irq_invalid_el2_64:
	handle_invalid_entry  IRQ_INVALID_EL2_64

fiq_invalid_el2_64:
	handle_invalid_entry  FIQ_INVALID_EL2_64

error_invalid_el2_64:
	handle_invalid_entry  ERROR_INVALID_EL2_64

sync_invalid_el2_32:
	handle_invalid_entry  SYNC_INVALID_EL2_32

irq_invalid_el2_32:
	handle_invalid_entry  IRQ_INVALID_EL2_32

fiq_invalid_el2_32:
	handle_invalid_entry  FIQ_INVALID_EL2_32

error_invalid_el2_32:
	handle_invalid_entry  ERROR_INVALID_EL2_32

sync_invalid_el2t:
	handle_invalid_entry  SYNC_INVALID_EL2t

irq_invalid_el2t:
	handle_invalid_entry  IRQ_INVALID_EL2t

fiq_invalid_el2t:
	handle_invalid_entry  FIQ_INVALID_EL2t

error_invalid_el2t:
	handle_invalid_entry  ERROR_INVALID_EL2t

sync_invalid_el2h:
	handle_invalid_entry  SYNC_INVALID_EL2h

irq_invalid_el2h:
	handle_invalid_entry  IRQ_INVALID_EL2h

fiq_invalid_el2h:
	handle_invalid_entry  FIQ_INVALID_EL2h

error_invalid_el2h:
	handle_invalid_entry  ERROR_INVALID_EL2h

sync_invalid_el1_64:
	handle_invalid_entry  SYNC_INVALID_EL1_64

irq_invalid_el1_64:
	handle_invalid_entry  IRQ_INVALID_EL1_64

fiq_invalid_el1_64:
	handle_invalid_entry  FIQ_INVALID_EL1_64

error_invalid_el1_64:
	handle_invalid_entry  ERROR_INVALID_EL1_64

sync_invalid_el1_32:
	handle_invalid_entry  SYNC_INVALID_EL1_32

irq_invalid_el1_32:
	handle_invalid_entry  IRQ_INVALID_EL1_32

fiq_invalid_el1_32:
	handle_invalid_entry  FIQ_INVALID_EL1_32

error_invalid_el1_32:
	handle_invalid_entry  ERROR_INVALID_EL1_32

sync_invalid_el1t:
	handle_invalid_entry  SYNC_INVALID_EL1t

irq_invalid_el1t:
	handle_invalid_entry  IRQ_INVALID_EL1t

fiq_invalid_el1t:
	handle_invalid_entry  FIQ_INVALID_EL1t

error_invalid_el1t:
	handle_invalid_entry  ERROR_INVALID_EL1t

sync_invalid_el1h:
	handle_invalid_entry  SYNC_INVALID_EL1h

irq_invalid_el1h:
	handle_invalid_entry  IRQ_INVALID_EL1h

fiq_invalid_el1h:
	handle_invalid_entry  FIQ_INVALID_EL1h

error_invalid_el1h:
	handle_invalid_entry  ERROR_INVALID_EL1h

sync_invalid_el0_64:
	handle_invalid_entry  SYNC_INVALID_EL0_64

irq_invalid_el0_64:
	handle_invalid_entry  IRQ_INVALID_EL0_64

fiq_invalid_el0_64:
	handle_invalid_entry  FIQ_INVALID_EL0_64

error_invalid_el0_64:
	handle_invalid_entry  ERROR_INVALID_EL0_64

sync_invalid_el0_32:
	handle_invalid_entry  SYNC_INVALID_EL0_32

irq_invalid_el0_32:
	handle_invalid_entry  IRQ_INVALID_EL0_32

fiq_invalid_el0_32:
	handle_invalid_entry  FIQ_INVALID_EL0_32

error_invalid_el0_32:
	handle_invalid_entry  ERROR_INVALID_EL0_32

el1_sync:
	handle_invalid_entry  SYNC_INVALID_EL1h
//	kernel_entry
//	mrs	x0, ESR_EL1
//	mrs	x1, FAR_EL1
//	bl	show_invalid_entry_message_el1_sync
//	b	err_hang

el1_irq:
	kernel_entry 
	bl	ukplat_irq_handle
	kernel_exit

.globl err_hang
err_hang: b err_hang


