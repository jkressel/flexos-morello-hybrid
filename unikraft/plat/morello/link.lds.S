/*
 * Copyright (C) 2018, bzt (bztsrc@github), https://github.com/bztsrc/raspi3-tutorial
 * Copyright (C) 2020, Santiago Pagani <santiagopagani@gmail.com>
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
#include <uk/arch/limits.h>
#include <uk/plat/common/common.lds.h>

#define RAM_BASE_ADDR	0x80000000
#define DTB_RESERVED_SIZE 0x100000

OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

SECTIONS
{
    . = RAM_BASE_ADDR;

	/* Place DTB binary at the beginning of the RAM */
	_dtb = .;
	. = . + DTB_RESERVED_SIZE;

	/* Code */
	_text = .;
	.text :
	{
		KEEP(*(.text.boot))
                *(.text)
		*(.text.*)
		*(.gnu.linkonce.t*)
	}
/* __FLEXOS MARKER__: insert compartment text sections here. */
	_etext = .;


	. = ALIGN(__PAGE_SIZE);
	EXCEPTION_SECTIONS

	CTORTAB_SECTION

	INITTAB_SECTION

	/* TODO Morello this probably needs to be inserted */

	_compartment_caps_start = .;
	.compartment_caps :
	{
		. = ALIGN(16);
		. += 2 * __PAGE_SIZE;
	}
	_compartment_caps_end = .;

	/* Read-only data */
	. = ALIGN(__PAGE_SIZE);
	_rodata = .;
	.rodata :
	{
		*(.rodata)
		*(.rodata.*)
		*(.gnu.linkonce.r*)
	}
	_erodata = .;

	/* Constructor tables (read-only) */
	. = ALIGN(0x10);
	_ctors = .;
	.preinit_array : {
		PROVIDE_HIDDEN (__preinit_array_start = .);
		KEEP (*(.preinit_array))
		PROVIDE_HIDDEN (__preinit_array_end = .);
	}

	. = ALIGN(0x10);
	.init_array : {
		PROVIDE_HIDDEN (__init_array_start = .);
		KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
		KEEP (*(.init_array .ctors))
		PROVIDE_HIDDEN (__init_array_end = .);
	}
	_ectors = .;

		/* Read-write data that is initialized explicitly in code */
	_data = .;
	.data :
	{
		*(.data_comp0)
		*(.data_comp0.*)
		*(.data)
		*(.data.*)
		*(.gnu.linkonce.d*)
		PROVIDE(flexos_comp0_alloc = .);
		. = . + (1000*__PAGE_SIZE);
		. = ALIGN(0x1000);
	}
	_edata = .;

	/* Read-write data that is not initialized explicitly in code */
	. = ALIGN(__PAGE_SIZE);
	__bss_start = .;
    .bss :
	{
		*(.bss_comp0)
		*(.bss_comp0.*)
		*(.bss)
		*(.bss.*)
        *(COMMON)
    }
	__bss_end = .;

	. = ALIGN(__PAGE_SIZE);
	TLS_SECTIONS

	__shared_data = .;
	.data_shared : {
		*(data_shared)
		*(.data_shared.*)
		PROVIDE(flexos_sd_alloc = .);
		/* here: enough to store the compartment's allocator
		 * FIXME derive from roundup(sizeof(struct uk_alloc)) */
		. = . + (1000*__PAGE_SIZE);
		. = ALIGN(0x1000);
	}
	__shared_data_end = .;


	/* -- compartment data sections begin -- */
			. = ALIGN(0x1000);
	_comp1 = .;
	.data_comp1 :
        {
		PROVIDE(flexos_comp1_alloc = .);
		/* here: enough to store the compartment's allocator
		 * FIXME derive from roundup(sizeof(struct uk_alloc)) */
		. = . + (1000*__PAGE_SIZE);
		. = ALIGN(0x1000);
		*(.data_comp1 .data_comp1.*)
		. = ALIGN(0x1000);
	}
	_ecomp1 = .;

    	. = ALIGN(0x1000);
	.initarray_comp1 :
        {
		*(.initarray_comp1 .initarray_comp1.*)
		. = ALIGN(0x1000);
	}
	_einitarray_comp1 = .;

	. += __bss_end - __bss_start;
	.bss_comp1 :
        {
		*(.bss_comp1 .bss_comp1.*)
		. = ALIGN(0x1000);
	}
	_ebss_comp1 = .;


		/* -- compartment data sections begin -- */
			. = ALIGN(0x1000);
	_comp2 = .;
	.data_comp2 :
        {
		PROVIDE(flexos_comp2_alloc = .);
		/* here: enough to store the compartment's allocator
		 * FIXME derive from roundup(sizeof(struct uk_alloc)) */
		. = . + (1000*__PAGE_SIZE);
		. = ALIGN(0x1000);
		*(.data_comp2 .data_comp2.*)
		. = ALIGN(0x1000);
	}
	_ecomp2 = .;

    	. = ALIGN(0x1000);
	_bss_comp2 = .;
	.initarray_comp2 :
        {
		*(.initarray_comp2 .initarray_comp2.*)
		. = ALIGN(0x1000);
	}
	_einitarray_comp2 = .;

	. += __bss_end - __bss_start;
	.bss_comp2 :
        {
		*(.bss_comp2 .bss_comp2.*)
		. = ALIGN(0x1000);
	}
	_ebss_comp2 = .;
	/* -- compartment data sections end -- */



    . = ALIGN(__PAGE_SIZE);
	. = ALIGN(16);
        __stack_start = .;
    .bss.stack (NOLOAD) :
        {
                . += 128 * __PAGE_SIZE;
    }
        __stack_end = .;

	. = ALIGN(__PAGE_SIZE);
	_pagetables = .;
	.pagetables (NOLOAD) :
	{
		. += 3 * __PAGE_SIZE;
	}

	_end = .;
	

	. = ALIGN(__PAGE_SIZE);
	.comment       0 : { *(.comment) }
	DEBUG_SYMBOLS

   /DISCARD/ : { *(.gnu*) *(.note*) }
}
__bss_size = (__bss_end - __bss_start)>>3;
