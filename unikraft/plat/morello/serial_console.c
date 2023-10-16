/*
 * Copyright (C) 2018, bzt (bztsrc@github), https://github.com/bztsrc/raspi3-tutorial
 * Copyright (C) 2020, Santiago Pagani <santiagopagani@gmail.com>
 * Copyright (c) 2022, John A. Kressel <jkressel.apps@gmail.com>
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
#include <stdint.h>

/* PL011 UART registers */
#define UART0_DR        ((volatile unsigned int*)(0x2A400000+0x0))
#define UART0_FR        ((volatile unsigned int*)(0x2A400000+0x18))
#define UART0_IBRD      ((volatile unsigned int*)(0x2A400000+0x24))
#define UART0_FBRD      ((volatile unsigned int*)(0x2A400000+0x28))
#define UART0_LCRH      ((volatile unsigned int*)(0x2A400000+0x2C))
#define UART0_CR        ((volatile unsigned int*)(0x2A400000+0x30))
#define UART0_IMSC      ((volatile unsigned int*)(0x2A400000+0x38))
#define UART0_ICR       ((volatile unsigned int*)(0x2A400000+0x44))

#define morello_create_capability_from_ptr(ptr, size, store_to_ptr)	\
	__asm__ volatile(	\
		"cvtp c0, %0\n"	\
		"scbnds c0, c0, %2\n"	\
		"mov x1, #(1<<1)\n"	\
		"clrperm c0, c0, x1\n"	\
		"str c0, [%1]\n"	\
		:	\
		: "r"((uintptr_t *)(ptr)), "r"((uintptr_t *)(store_to_ptr)), "r"(size)	\
		: "c0", "x1", "memory"	\
	)

static char prev_sent_char = '\0';

void *__capability uart_cap;

static void wait_cycles(unsigned int n)
{
    if (n) {
		while (n--) {
			asm volatile("nop");
		}
	}
}

unsigned int serial_tx_buffer_full(void)
{
//	return *UART0_FR&0x20;
  uint32_t result = 0;
  __asm__ volatile(
    "mov w0, %2 \n"
    "ldr w1, [%0, #0x18]\n"
    "and w1, w1, w0\n"
    "str w1, [%1]\n"
    :
    : "r"(uart_cap), "r"(&result), "i"(0x20)
    : "x0", "x1"
  );
  return result;
}

static unsigned int serial_rx_buffer_empty(void)
{
	return *UART0_FR&0x10;
}

/**
 * Set baud rate and characteristics (115200 8N1)
 */
void _libmorelloplat_init_serial_console()
{
    register unsigned int r;

    /* initialize UART */
    *UART0_CR = 0;         // turn off UART0
    // *UART0_LCRH = 1<<4;

    *UART0_ICR = 0x7FF;    // clear interrupts
    /*
    *
    * IBRD = 27
    * FBRD = 9
    *
    * Calculation:
    *   UART_CLK = 50MHz
    *   Baud rate divisor = (50 x (10^6)) / (16 * 115200) = 27.12673611
    *
    *   Therefore, IBRD is 27, for the fractional part, we do the following:
    *     ((0.12673611 * 64) + 0.5) = 9 (as integer)
    *   So FBRD is 9
    * 
    *
    */
    *UART0_IBRD = (uint32_t)0x1B;       // 115200 baud
    *UART0_FBRD = (uint32_t)0x8;
    *UART0_LCRH = (uint32_t)0b11<<5; // 8n1
    *UART0_CR = (uint32_t)0x301;     // enable Tx, Rx, FIFO

    morello_create_capability_from_ptr((uintptr_t)0x2A400000, (uintptr_t)0x60, ((uintptr_t *)(&(uart_cap))));

}

/**
 * Send a character
 */
void _libmorelloplat_serial_putc(char c)
{
	if ((c == '\n') && (prev_sent_char != '\r'))
		_libmorelloplat_serial_putc('\r');

//    int doit = 0;
//    while(doit < 10000000) {
//      asm volatile ("nop\n");
//      doit++;
//    }
    
    // Wait until we can send
    do{
		asm volatile("nop");
	} while (serial_tx_buffer_full());

    // Write the character to the buffer
//    *UART0_DR = c;
  __asm__ volatile(
    "mov x0, %1 \n"
    "str w0, [%0, #0x4]\n"
    "str w0, [%0]\n"
    :
    : "r"(uart_cap), "r"(c)
    : "x0"
  );
	prev_sent_char = c;
}

/**
 * Receive a character
 */
int  _libmorelloplat_serial_getc(void)
{
	if (serial_rx_buffer_empty())
		return -1;

    char r;
    r = (char)(*UART0_DR);
    return (int)r;
}
