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

#include <flexos/isolation.h>
#include <flexos/example/isolated.h>

#include <uk/sched.h>

void pong1(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	flexos_nop_gate(0, 1, ping1, arg1 - 1, arg2, arg3, arg4, arg5, arg6);
}

void pong2(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	flexos_nop_gate(0, 1, ping2, arg1, arg2 - 1, arg3, arg4, arg5, arg6);
}

void pong3(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	flexos_nop_gate(0, 1, ping3, arg1, arg2, arg3 - 1, arg4, arg5, arg6);
}

void pong4(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	flexos_nop_gate(0, 1, ping4, arg1, arg2, arg3, arg4 - 1, arg5, arg6);
}

void pong5(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	flexos_nop_gate(0, 1, ping5, arg1, arg2, arg3, arg4, arg5 - 1, arg6);
}

void pong6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	flexos_nop_gate(0, 1, ping6, arg1, arg2, arg3, arg4, arg5, arg6 - 1);
}

unsigned int fib0(unsigned int n)
{
	if (n <= 1) {
		return n;
	}
	unsigned int f1, f2;
	flexos_nop_gate_r(0, 1, f1, fib1, n - 1);
	flexos_nop_gate_r(0, 1, f2, fib1, n - 2);
	return f1 + f2;
}

void yield_func(int n)
{
	for (int i = 0; i < n; ++i) {
		uk_sched_yield();
	}
}
