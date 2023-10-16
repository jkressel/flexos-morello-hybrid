/*
 * Copyright (C) 2018, bzt (bztsrc@github), https://github.com/bztsrc/raspi3-tutorial
 * Copyright (c) 2018, Sergey Matyukevich <https://github.com/s-matyukevich/raspberry-pi-os>
 *           (c) 2020, Santiago Pagani <santiagopagani@gmail.com>
 * Copyright (c) 2022, John A. Kressel <jkressel.apps@gmail.com>
 *
 *
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

#include <morello/sysregs.h>
#include <morello/mm.h>
#include <morello/mmu.h>
#include <uk/config.h>

.section ".text.boot"
.global _start
_start:
b setup_cpu

hang:  wfe
    b       hang


setup_cpu:

/*
* First make sure that morello features don't get trapped
* What controls access to morello I hear you ask? 
* From the Morello architecture specification:
* 1. cptr_el3, EC (bit 9): 0b0 causes traps, 0b1 no trap to el3
* 2. cptr_el2, CEN (bits 18 and 19): bit pattern 0b11 causes no traps to el2
* 3. cptr_el2, TC (bit 9): 0b0 causes no morello instructions to be trapped
* 4. cpacr_el1, CEN (bits 18 and 19): 0b11 causes no instruction traps
* Also while I'm at it, disable fp traps at el1.
* 
*/

	ldr x0, =((0x3ff & 0x11)|1<<27)
	msr pmevtyper0_el0, x0
	ldr  x0, =(1<<31)
	msr pmintenclr_el1, x0
	ldr  x1, =(1<<0|1<<2)
	mrs x0, pmcr_el0
	orr x0, x0, x1
	msr pmcr_el0, x0

	// mov x0, #6
	// ldr x1, =(1<<7)
	// orr x0, x0, x1
	// msr MDCR_EL2, x0


	ldr 	   x0, =(1 << 22 | 1 << 21 | 1<<28 | 1<<27)
	msr        PMCCFILTR_EL0, x0

	ldr  x0, =(1<<31)
	msr pmcntenset_el0, x0
	ldr  x0, =(1<<0 | 1<<2)
	msr pmuserenr_el0, x0

	mrs	x9, MDCR_EL3
	orr	x9, x9, #(1 << 17)
	msr	MDCR_EL3, x9

	// first pmu counters
	mov	x9, #0
	msr	PMSELR_EL0, x9
	isb

	mov	x10, #0x14		// L1I_Cache
//	mov x10, #0x04		// L1D_cache
//	mov x10, #0x08 		// Inst retired
//	mov x10, #0x21		// br ret
	msr	PMXEVTYPER_EL0, x10
	isb

	mov	x9, xzr
	orr	x9, x9, #(1 << 0)
	msr	PMCNTENSET_EL0, x9
	isb

		// second pmu counters
	mov	x9, #1
	msr	PMSELR_EL0, x9
	isb

	mov	x10, #0x01		// L1I_cache_refill
//	mov x10, #0x03      // L1D_cache_refill
//	mov x10, #0x13 		// mem access
//	mov x10, #0x22 		// br ret mis pred
	msr	PMXEVTYPER_EL0, x10
	isb

	mov	x9, xzr
	orr	x9, x9, #(1 << 1)
	msr	PMCNTENSET_EL0, x9
	isb
	// enable, reset
	mrs	x9, PMCR_EL0
	orr	w9, w9, #(1 << 0)
	orr	w9, w9, #(1 << 1)
	orr	w9, w9, #(1 << 2)
	msr	PMCR_EL0, x9
	isb
	// mov x0, #0
	// msr PMSELR_EL0, x0

	// mrs x0, pmcntenset_el0
	// orr x0, x0, #1
	// msr pmcntenset_el0, x0




	ldr        x0, =(1 << 9) // EC (bit 9) is 1     
    msr        cptr_el3, x0

	ldr        x0, =(3 << 18) //CEN (bits 18 and 19) are 1 and TC (bit 9) is 0
	msr        cptr_el2, x0


	ldr x1, =(3 << 20) //fp
	ldr x2, =(3 << 18) // CEN (bits 18 and 19) set to 1
	orr x0, x1, x0
	orr x0, x0, x2
	msr		cpacr_el1, x0 //enable fp and capabilities


	isb


	// Set up VMPIDR_EL2/VPIDR_EL1
	// ---------------------------
 	mrs   x0, midr_el1
 	msr   vpidr_el2, x0
 	mrs   x0, mpidr_el1
 	msr   vmpidr_el2, x0


	// Set up exception vectors	
	// --------------------------------
	ldr      x0, =vectors_el1
	cvtp     c0, x0
  	msr      cvbar_el1, c0

	ldr      x0, =vectors_el2
	cvtp     c0, x0
  	msr      cvbar_el2, c0

	ldr      x0, =vectors_el3
	cvtp     c0, x0
  	msr      cvbar_el3, c0
 
 
	// Set VMID
	// ---------
	// Although we are not using stage 2 translation, NS.EL1 still cares
	// about the VMID
	msr   vttbr_el2, xzr


	// Set SCTLRs for EL1/2 to safe values
	// ------------------------------------
	msr   sctlr_el2, xzr
	msr   sctlr_el1, xzr


	//set clock freq
	ldr	  x0, =(2400000000)
	msr   cntfrq_el0, x0

	// Configure HCR_EL2
	// ------------------
	orr      w0, wzr, #(1 << 3)               // FMO=1
	orr      x0, x0,  #(1 << 4)               // IMO=1
	orr      x0, x0,  #(1 << 31)              // RW=1          NS.EL1 is AArch64
												// TGE=0         Entry to NS.EL1 is possible
												// VM=0          Stage 2 MMU disabled
	msr      hcr_el2, x0





	// Set the SPSR state to restore when returning from EL3 to EL1
	// ------------------------------------------------------------
	ldr		x0, =(0x05)
	msr		spsr_el3, x0

	ldr		x0, =(0x05)
	msr		spsr_el1, x0

	ldr		x0, =(el1_entry)
	cvtp    c0, x0
	msr		celr_el3, c0

	mrs     x0, cctlr_el1
	orr     x0, x0, #(0x1 << 0)
	bic 	x0, x0, #(0x1 << 7) //don't want return caps to be sealed, you may want this behaviour enabled though
//	orr     x0, x0, #(0x1 << 2)
//	orr     x0, x0, #(0x1 << 3)
	msr     cctlr_el1, x0

//	mov     x0, #(0x3 << 0)
//	msr     cctlr_el2, x0

//	mov     x0, #(0x1 << 0)
//	msr     cctlr_el3, x0


	eret  //jump down to el1



//We are now at EL1
el1_entry:


	ldr   x0, =_pagetables
	msr   ttbr0_el1, x0   

	//mrs      c0, cvbar_el1       


	// Set up memory attributes
	// -------------------------
	// This equates to:
	// 0 = b01000100 = Normal, Inner/Outer Non-Cacheable
	// 1 = b11111111 = Normal, Inner/Outer WB/WA/RA
	// 2 = b00000000 = Device-nGnRnE
	mov   x0, #0x000000000000FF44
	msr   mair_el1, x0



	// Set up TCR_EL1
	// ---------------
	mov   x0, #0x19             // T0SZ=0b011001 Limits VA space to 39 bits
	orr   x0, x0, #(0x1 << 8)   // IGRN0=0b01  Walks to TTBR0 are Inner WB/WA
	orr   x0, x0, #(0x1 << 10)  // OGRN0=0b01  Walks to TTBR0 are Outer WB/WA
	orr   x0, x0, #(0x3 << 12)  // SH0=0b11   Inner Shareable
	orr   x0, x0, #(0x1 << 23)  // EPD1=0b1   Disable table walks from TTBR1
	orr   x0, x0, #(0x1 << 41)  // Don't fault capability loads and stores
	orr   x0, x0, #(0x1 << 44)  // Don't fault capability stores
	orr   x0, x0, #(0x1 << 45)  // Don't fault capability loads
	orr   x0, x0, #(0x1 << 60)  // Don't fault capability stores
	orr   x0, x0, #(0x1 << 61)	//Don't fault capability loads
								// TBI0=0b0
								// TG0=0b00   4KB granule for TTBR0
								// A1=0     TTBR0 contains the ASID
								// AS=0     8-bit ASID
								// IPS=0     32-bit IPA space

	mov x9, x0
	msr   tcr_el1, x0


	dsb   sy
	isb




	//Set up blocks

	ldr   x1, =_pagetables


	// [0]: 0x0000,0000 - 0x3FFF,FFFF
	ldr      x0, =0x00600000000000409          // Entry template
												// Don't need to OR in address, as it is 0
												// AP=0b00, EL1 RW, EL0 No Access
	str      x0, [x1]


	
	// [1]: 0x4000,0000 - 0x7FFF,FFFF
	ldr      x0, =0x00600000000000409          // Entry template
	orr      x0, x0, #0x40000000               // 'OR' template with base physical address
												// AP=0b00, EL1 RW, EL0 No Access
	str      x0, [x1, #8]



	// [2]: 0x8000,0000 - 0xBFFF,FFFF (DRAM on the VE and Base Platform)
	ldr      x0, =0x3000000000000405           // Entry template
	orr      x0, x0, #0x80000000               // 'OR' template with base physical address
												// AP=0b00, EL1 RW, EL0 No Access
	str      x0, [x1, #16]



	// [3]: 0xC0000000 - 0xFFFF,FFFF (DRAM on the VE and Base Platform)
	ldr      x0, =0x3000000000000405            // Entry template
	orr      x0, x0, #0xC0000000               // 'OR' template with base physical address
												// AP=0b00, EL1 RW, EL0 No Access
	str      x0, [x1, #24]

	dsb      sy


 	mov      x0, #(1 << 0)                     // M=1           Enable the stage 1 MMU
  	orr      x0, x0, #(1 << 2)                 // C=1           Enable data and unified caches
  	orr      x0, x0, #(1 << 12)                // I=1           Enable instruction fetches to allocate into unified caches
                                             // A=0           Strict alignment checking disabled
                                             // SA=0          Stack alignment checking disabled
                                             // WXN=0         Write permission does not imply XN
                                             // EE=0          EL3 data accesses are little endian
	msr      SCTLR_EL1, x0
	isb

	//
	// MMU is now enabled
	//

	nop
	nop
	nop
	nop


	//setup stack pointer to use
	msr             SPSel, #1
	ldr             x1, =__stack_end
	and             x1, x1, #-16
	mov             sp, x1



	clear_bss_start:
		// Clear bss
		ldr     x1, =__bss_start
		ldr     w2, =__bss_size
	clear_bss_loop:
		cbz     w2, clear_bss_done
		str     xzr, [x1], #8
		sub     w2, w2, #1
		cbnz    w2, clear_bss_loop
	clear_bss_done:



jump_to_C:
	//load c entry point
	ldr		x4, =_libmorelloplat_entry
	br 		x4

	// As a failsafe, we also hang the main core
    b       hang
