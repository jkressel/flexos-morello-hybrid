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

#include <stdio.h>
#include <flexos/isolation.h>
#include <flexos/example/isolated.h>
#include <uk/alloc.h>
#include <stdlib.h>
#include <flexos/impl/main_annotation.h>

int main(int __unused argc, char __unused *argv[])
{
	uk_pr_info("main thread is %p\n", uk_thread_current());

	/* Test gate reentrance */

	int ret = 0;
#if CONFIG_LIBFLEXOS_INTELPKU
	wrpkru(0x0);
	struct uk_thread_status_block tsb_comp0_cpy[32];
	memcpy(tsb_comp0_cpy, tsb_comp0, 32 * sizeof(struct uk_thread_status_block));
	struct uk_thread_status_block tsb_comp1_cpy[32];
	memcpy(tsb_comp1_cpy, tsb_comp1, 32 * sizeof(struct uk_thread_status_block));
	wrpkru(0x3ffffffc);
#endif

#define FELXOS_TEST_RECURSION_DEPTH 20
	uk_pr_info("Testing reentrance with recursion depth %d.\n", FELXOS_TEST_RECURSION_DEPTH);

	flexos_gate_r(libflexosexample, ret, ping1, FELXOS_TEST_RECURSION_DEPTH, 2, 3, 4, 5, 6);
	flexos_gate(libflexosexample, reset_runs);
	if (ret != FELXOS_TEST_RECURSION_DEPTH) {
		uk_pr_err("expected %d but got %d\n", FELXOS_TEST_RECURSION_DEPTH, ret);
		UK_CRASH("PING1: FAILURE\n");
	}
	uk_pr_info("PING1: SUCCESS\n");

	flexos_gate_r(libflexosexample, ret, ping2, 1, FELXOS_TEST_RECURSION_DEPTH, 3, 4, 5, 6);
	flexos_gate(libflexosexample, reset_runs);
	if (ret != FELXOS_TEST_RECURSION_DEPTH) {
		UK_CRASH("PING2: FAILURE\n");
	}
	uk_pr_info("PING2: SUCCESS\n");

	flexos_gate_r(libflexosexample, ret, ping3, 1, 2, FELXOS_TEST_RECURSION_DEPTH, 4, 5, 6);
	flexos_gate(libflexosexample, reset_runs);
	if (ret != FELXOS_TEST_RECURSION_DEPTH) {
		UK_CRASH("PING3: FAILURE\n");
	}
	uk_pr_info("PING3: SUCCESS\n");

	flexos_gate_r(libflexosexample, ret, ping4, 1, 2, 3, FELXOS_TEST_RECURSION_DEPTH, 5, 6);
	flexos_gate(libflexosexample, reset_runs);
	if (ret != FELXOS_TEST_RECURSION_DEPTH) {
		UK_CRASH("PING4: FAILURE\n");
	}
	uk_pr_info("PING4: SUCCESS\n");

	flexos_gate_r(libflexosexample, ret, ping5, 1, 2, 3, 4, FELXOS_TEST_RECURSION_DEPTH, 6);
	flexos_gate(libflexosexample, reset_runs);
	if (ret != FELXOS_TEST_RECURSION_DEPTH) {
		UK_CRASH("PING5: FAILURE\n");
	}
	uk_pr_info("PING5: SUCCESS\n");

	flexos_gate_r(libflexosexample, ret, ping6, 1, 2, 3, 4, 5, FELXOS_TEST_RECURSION_DEPTH);
	flexos_gate(libflexosexample, reset_runs);
	if (ret != FELXOS_TEST_RECURSION_DEPTH) {
		UK_CRASH("PING6: FAILURE\n");
	}
	uk_pr_info("PING6: SUCCESS\n");

	return 0;
#if CONFIG_LIBFLEXOS_INTELPKU
	wrpkru(0x0);
	for (int i = 0; i < 32; i++) {
		if (tsb_comp0[i].sp != tsb_comp0_cpy[i].sp) {
			UK_CRASH("TSB CHECK: FAILURE: tsb_comp0[%d].sp (%p) != tsb_comp0_cpy[%d].sp (%p)\n",
					i, tsb_comp0[i].sp, i, tsb_comp0_cpy[i].sp);
		}
		if (tsb_comp0[i].bp != tsb_comp0_cpy[i].bp) {
			UK_CRASH("TSB CHECK: FAILURE: tsb_comp0[%d].bp (%p) != tsb_comp0_cpy[%d].bp (%p)\n",
					i, tsb_comp0[i].bp, i, tsb_comp0_cpy[i].bp);
		}
	}
	for (int i = 0; i < 32; i++) {
		if (tsb_comp1[i].sp != tsb_comp1_cpy[i].sp) {
			UK_CRASH("TSB CHECK: FAILURE: tsb_comp1[%d].sp (%p) != tsb_comp1_cpy[%d].sp (%p)\n",
					i, tsb_comp1[i].sp, i, tsb_comp1_cpy[i].sp);
		}
		if (tsb_comp1[i].bp != tsb_comp1_cpy[i].bp) {
			UK_CRASH("TSB CHECK: FAILURE: tsb_comp1[%d].bp (%p) != tsb_comp1_cpy[%d].bp (%p)\n",
					i, tsb_comp1[i].bp, i, tsb_comp1_cpy[i].bp);
		}
	}
	wrpkru(0x3ffffffc);
	uk_pr_info("TSB CHECK: SUCCESS\n");
#endif
}
