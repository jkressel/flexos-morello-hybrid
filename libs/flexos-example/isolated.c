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

/* No need to include flexos/isolation.h for this example since we don't call
 * any gate, but let's put it for good measure. */
#include <flexos/isolation.h>

#include <flexos/example/isolated.h>
#include <stdio.h>

#include <uk/sched.h>
#include <uk/thread.h>

/* note: these buffers are volatile so that accesses don't get optimized away in
 * this example
 */

/* the static buffer that we return from function2: has to be shared */
/* FIXME FLEXOS: use __section and not __attribute__((flexos_whitelist)) because
 * of a bug in Coccinelle. Revisit this later. */
static char static_buf[128] __section(".data_shared")
	= "Aux meilleurs esprits, Que d'erreurs promises!";

/* a private static buffer: only accessible from this compartment */
static char static_lib_secret[32] __section(".data_comp1") = "abcdefghijklmnopqrstuvwxyz";

static int compute_signature(char *buf)
{
	/* do some crypto stuff, touch static_lib_secret */
	return static_lib_secret[0] + buf[0];
}

/* FIXME FLEXOS: if we simply put these string literals in the code, they will
 * be stored under the microlib's section, leading to a protection domain fault
 * when printing. We have to find a way to automatically put them in the shared
 * section.
 */
static char nullbuf[100]  = "buf is NULL!\n";
static char zerobuf[100] __section(".data_comp1") = "buf is 0!\n";

int function11()
{
	// if (!buf) {
	// 	flexos_nop_gate(1, 0, printf, &nullbuf[0]);
	// 	/* should not happen, bug in the toolchain? */
	// 	return 1;
	// }

	// if (buf[0] == 0) {
	// 	flexos_nop_gate(1, 0, printf, &zerobuf[0]);
	// 	/* should not happen, bug in the toolchain? */
	// 	return 2;
	// }

	// /* note: this should crash if we do not have access to buf */
	// compute_signature(buf);
	char loaded;
	//__asm__ volatile("nop\n"::"r"(static_buf[7]):"memory");
	MORELLO_LOAD_SHARED_DATA(static_buf, loaded);
	//loaded = static_buf[0];
	*static_lib_secret = loaded;
	__asm__ volatile("nop\n"::"r"(static_lib_secret):"memory");
	char computed_sig = compute_signature(zerobuf);

	return computed_sig;
}





// int isolated_func1(char *__capability buffer) 
// {
// 	char* local_buf = uk_malloc(comp1_allocator, 42 * sizeof(char));
// 	local_buf[30] = buffer[30];

// 	return 0;
// }

// int call_isolated_func1(char* buffer) {
// 	int ret;
// //	_flexos_morello_gate_r(1, 0, 1, ret, isolated_func1, (char* __capability)buffer);
// 	return ret;
// }

char *function2(void)
{
	return static_buf;
}

static int runs = 0;

void reset_runs(void) {
	runs = 0;
}

void pong1(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
void pong2(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
void pong3(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
void pong4(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
void pong5(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
void pong6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);

int ping1(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	if (arg1) {
		runs++;
		flexos_nop_gate(1, 0, pong1, arg1, arg2, arg3, arg4, arg5,
				arg6);
	}
	return runs;
}

int ping2(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	if (arg2) {
		runs++;
		flexos_nop_gate(1, 0, pong2, arg1, arg2, arg3, arg4, arg5,
				arg6);
	}
	return runs;
}

int ping3(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	if (arg3) {
		runs++;
		flexos_nop_gate(1, 0, pong3, arg1, arg2, arg3, arg4, arg5,
				arg6);
	}
	return runs;
}

int ping4(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	if (arg4) {
		runs++;
		flexos_nop_gate(1, 0, pong4, arg1, arg2, arg3, arg4, arg5,
				arg6);
	}
	return runs;
}

int ping5(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	if (arg5) {
		runs++;
		flexos_nop_gate(1, 0, pong5, arg1, arg2, arg3, arg4, arg5,
				arg6);
	}
	return runs;
}

int ping6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6)
{
	if (arg6) {
		runs++;
		flexos_nop_gate(1, 0, pong6, arg1, arg2, arg3, arg4, arg5,
				arg6);
	}
	return runs;
}

extern unsigned int fib0(unsigned int n);

extern void set_lib_test_finished(int x);
extern int is_lib_test_finished();

extern void yield_func(int n);

char c_buf00[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf01[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf02[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf03[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf04[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf05[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf06[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf07[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf08[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
char c_buf09[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];

char *buffers[10] = {
	&c_buf00[0], &c_buf01[0], &c_buf02[0], &c_buf03[0], &c_buf04[0],
	&c_buf05[0], &c_buf06[0], &c_buf07[0], &c_buf08[0], &c_buf09[0],
};

void write_to_buf(size_t buf_index, size_t i, char byte)
{
	UK_ASSERT(buf_index < sizeof(buffers) / sizeof(char*));
	UK_ASSERT(i < FLEXOS_TEST_CONCURRENCY_BUF_SIZE);
	char *buf = buffers[buf_index];
	buf[i] = byte;
}

char read_from_buf(size_t buf_index, size_t i)
{
	UK_ASSERT(buf_index < sizeof(buffers) / sizeof(char*));
	UK_ASSERT(i < FLEXOS_TEST_CONCURRENCY_BUF_SIZE);
	char *buf = buffers[buf_index];
	return buf[i];
}

void print_buffer(size_t buffer_index)
{
	if (buffer_index >= sizeof(buffers) / sizeof(char*)) {
		UK_CRASH("Invalid buffer index.\n");
	}

	char *buf = buffers[buffer_index];
	uk_pr_info("Content of buffer %d: %s\n", buffer_index, buf);
}

unsigned int fib1(unsigned int n)
{
	if (n <= 1) {
		return n;
	};
	unsigned int f1, f2;
	flexos_nop_gate_r(1, 0, f1, fib0, n - 1);
	flexos_nop_gate_r(1, 0, f2, fib0, n - 2);
	return f1 + f2;
}


struct worker_info_rec {
	int *fib_requests;
	int *fib_results;
};

static void *thread_func_lib_recursive(void *arg)
{
//	int tid = uk_thread_current()->tid;
//	int worker_id = tid - FLEXOS_TEST_CONCURRENCY_TID_BIAS;

//	struct worker_info_rec *winfo = (struct thread_func_recursive*) arg;
//	int fib_req = winfo->fib_requests[worker_id];
//	int *fib_result = &winfo->fib_results[worker_id];

//	*fib_result = fib1(fib_req);

//	return NULL;
}

static void concurrency_test_lib_recursive()
{
	uk_pr_info(">>>>>>>> Starting recursive concurrency test (lib)  <<<<<<<<\n");

	/* max 10 threads */
	uint8_t thread_counts[] = {1, 2, 3, 4, 5, 10};
	struct uk_thread *current = uk_thread_current();
	struct uk_thread *threads[FLEXOS_TEST_CONCURRENCY_MAX_THREADS];

	unsigned int fib_seq[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610};
	unsigned int fib_requests[] = {3, 4, 5, 3, 4, 5, 3, 4, 5, 3};
	unsigned int fib_results[FLEXOS_TEST_CONCURRENCY_MAX_THREADS];
	struct worker_info_rec winfo = {.fib_requests = fib_requests, .fib_results = fib_results};

	for (size_t i = 0; i < sizeof(thread_counts); ++i) {
		uint8_t thread_cnt = thread_counts[i];
		for (uint8_t j = 0; j < thread_cnt; ++j) {
			threads[j] = uk_sched_thread_create(current->sched, NULL, NULL, &thread_func_lib_recursive, &winfo);
		}

		for (uint8_t j = 0; j < thread_cnt; ++j) {
			volatile struct uk_thread *t = threads[j];
			while (!is_exited(t)) {
				//uk_pr_info("main thread waiting for worker %d\n", j);
				uk_sched_yield();
			}
			uk_sched_thread_destroy(current->sched, t);
		}

		for (uint8_t j = 0; j < thread_cnt; ++j) {
			if (fib_results[j] != fib_seq[fib_requests[j]]) {
				UK_CRASH("recursive concurrency test (lib): FAILED (thread_cnt=%d), %d->%d", thread_cnt, fib_requests[j], fib_results[j]);
			}
		}
	}
	uk_pr_info("recursive concurrency test (lib): SUCCESS (thread counts: %d", thread_counts[0]);
	for (size_t i = 1; i < sizeof(thread_counts); ++i) {
		uk_pr_info(", %d", thread_counts[i]);
	}
	uk_pr_info(")\n");
}

static void *thread_func_lib_yield(void *arg)
{
	flexos_nop_gate(1, 0, yield_func, 10);
	return NULL;
}

static void yield_test_lib()
{
	uk_pr_info(">>>>>>>> Starting yield test (lib) <<<<<<<<\n");

	uint8_t thread_counts[] = {1, 2, 3, 4, 5, 10};
	struct uk_thread *current = uk_thread_current();
	struct uk_thread *threads[FLEXOS_TEST_CONCURRENCY_MAX_THREADS];

	for (size_t i = 0; i < sizeof(thread_counts); ++i) {
		uint8_t thread_cnt = thread_counts[i];
		for (uint8_t j = 0; j < thread_cnt; ++j) {
			threads[j] = uk_sched_thread_create(current->sched, NULL, NULL, &thread_func_lib_yield, NULL);
		}

		for (uint8_t j = 0; j < thread_cnt; ++j) {
			volatile struct uk_thread *t = threads[j];
			while (!is_exited(t)) {
				uk_sched_yield();
				uk_pr_info("waiting for thread %d\n", j);
			}
			uk_sched_thread_destroy(current->sched, t);
		}
	}
	uk_pr_info("yield test (lib): SUCCESS\n");
}

void *lib_test_main(void *arg)
{
//	concurrency_test_lib_recursive();
//	yield_test_lib();
	return NULL;
}

static volatile struct uk_thread *main_thread;

int is_main_thread_exited()
{
	if (!main_thread) {
		return 1;
	}
	if (is_exited(main_thread)) {
		uk_sched_thread_destroy(uk_thread_current()->sched, main_thread);
		main_thread = NULL;
		return 1;
	}
	return 0;
}

void lib_test_start()
{
	uk_pr_info("\nStarting test (lib)\n");
	struct uk_thread *current = uk_thread_current();
	main_thread = uk_sched_thread_create(current->sched, NULL, NULL, lib_test_main, NULL);
}
