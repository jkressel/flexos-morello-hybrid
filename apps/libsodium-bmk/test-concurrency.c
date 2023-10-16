/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2021, Sebastian Rauch <s.rauch94@gmail.com>
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
#include <uk/alloc.h>
#include <uk/sched.h>
#include <uk/thread.h>
#include <stdlib.h>
#include <flexos/impl/main_annotation.h>

#if !CONFIG_LIBFLEXOS_VMEPT
	#error "This test is VM/EPT specific."
#endif /* CONFIG_LIBFLEXOS_VMEPT */

static void concurrency_test_simple();
static void concurrency_test_recursive();
static void yield_test();

/* must be between 1 and 10 */
#define FLEXOS_TEST_CONCURRENCY_NUM_THREADS 7

/* how many bytes to copy */
#define FLEXOS_TEST_CONCURRENCY_CPY_LENGTH 20

UK_CTASSERT(FLEXOS_TEST_CONCURRENCY_CPY_LENGTH <= FLEXOS_TEST_CONCURRENCY_BUF_SIZE);

volatile int thread_finished[FLEXOS_TEST_CONCURRENCY_NUM_THREADS];

static int cmp_strings(char *s, char *t, size_t max_len)
{
	for (size_t i = 0; i < max_len; ++i) {
		if (s[i] == 0 &&  t[i] == 0) {
			return 0;
		} else if (s[i] != t[i]) {
			return 1;
		}
	}
	return 0;
}

int main(int __unused argc, char __unused *argv[])
{
	concurrency_test_simple();
	concurrency_test_recursive();
	yield_test();

	flexos_gate(libflexosexample, lib_test_start);

	while (1) {
		volatile int exited;
		flexos_gate_r(libflexosexample, exited, is_main_thread_exited);
		if (exited) {
			break;
		}
		uk_pr_info("waiting for lib test to finish\n");
		uk_sched_yield();
	}
}

struct worker_info_simple {
	char **strings;
	size_t copy_length;
};

static void *thread_func_simple(void *arg)
{
	int tid = uk_thread_current()->tid;
	int worker_id = tid - FLEXOS_TEST_CONCURRENCY_TID_BIAS;

	struct worker_info_simple *winfo = (struct worker_info_simple*) arg;
	char *string = winfo->strings[worker_id];

	for (size_t i = 0; i < winfo->copy_length; ++i) {
		flexos_gate(libflexosexample, write_to_buf, worker_id, i, string[i]);
	}

	return NULL;
}

static void concurrency_test_simple()
{
	uk_pr_info(">>>>>>>> Starting simple concurrency test <<<<<<<<\n");
	const char *strings[] = {
		"Worker thread 00.",
		"Worker thread 01.",
		"Worker thread 02.",
		"Worker thread 03.",
		"Worker thread 04.",
		"Worker thread 05.",
		"Worker thread 06.",
		"Worker thread 07.",
		"Worker thread 08.",
		"Worker thread 09."
	};

	uint8_t thread_counts[] = {1, 2, 3, 4, 5, 10};
	struct uk_thread *current = uk_thread_current();
	struct uk_thread *threads[FLEXOS_TEST_CONCURRENCY_MAX_THREADS];
	struct worker_info_simple winfo = {.strings = &strings, .copy_length = 20};
	for (size_t i = 0; i < sizeof(thread_counts); ++i) {
		uint8_t thread_cnt = thread_counts[i];
		for (uint8_t j = 0; j < thread_cnt; ++j) {
			threads[j] = uk_sched_thread_create(current->sched, NULL, NULL, &thread_func_simple, &winfo);
		}

		for (uint8_t j = 0; j < thread_cnt; ++j) {
			volatile struct uk_thread *t = threads[j];
			while (!is_exited(t)) {
				//uk_pr_info("main thread waiting for worker %d\n", j);
				uk_sched_yield();
			}
			uk_sched_thread_destroy(current->sched, t);
		}

		char tmp_buffer[FLEXOS_TEST_CONCURRENCY_BUF_SIZE];
		for (uint8_t j = 0; j < thread_cnt; ++j) {
			for (size_t k = 0; k < winfo.copy_length; ++k) {
				flexos_gate_r(libflexosexample, tmp_buffer[k], read_from_buf, j, k);
			}
			if (cmp_strings(tmp_buffer, strings[j], FLEXOS_TEST_CONCURRENCY_BUF_SIZE)) {
				UK_CRASH("simple concurrency test: FAILED (thread_cnt=%d)", thread_cnt);
			}
		}
	}
	uk_pr_info("simple concurrency test: SUCCESS (thread counts: %d", thread_counts[0]);
	for (size_t i = 1; i < sizeof(thread_counts); ++i) {
		uk_pr_info(", %d", thread_counts[i]);
	}
	uk_pr_info(")\n");
}

struct worker_info_rec {
	int *fib_requests;
	int *fib_results;
};

static void *thread_func_recursive(void *arg)
{
	int tid = uk_thread_current()->tid;
	int worker_id = tid - FLEXOS_TEST_CONCURRENCY_TID_BIAS;

	struct worker_info_rec *winfo = (struct thread_func_recursive*) arg;
	int fib_req = winfo->fib_requests[worker_id];
	int *fib_result = &winfo->fib_results[worker_id];

	*fib_result = fib0(fib_req);

	return NULL;
}

static void concurrency_test_recursive()
{
	uk_pr_info(">>>>>>>> Starting recursive concurrency test <<<<<<<<\n");

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
			threads[j] = uk_sched_thread_create(current->sched, NULL, NULL, &thread_func_recursive, &winfo);
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
				UK_CRASH("recursive concurrency test: FAILED (thread_cnt=%d), %d->%d", thread_cnt, fib_requests[j], fib_results[j]);
			}
		}
	}
	uk_pr_info("recursive concurrency test: SUCCESS (thread counts: %d", thread_counts[0]);
	for (size_t i = 1; i < sizeof(thread_counts); ++i) {
		uk_pr_info(", %d", thread_counts[i]);
	}
	uk_pr_info(")\n");
}

extern void yield_func(int n);

static void *thread_func_yield(void *arg)
{
	flexos_gate(libflexosexample, yield_func, 10);
	return NULL;
}

static void yield_test()
{
	uk_pr_info(">>>>>>>> Starting yield test <<<<<<<<\n");

	uint8_t thread_counts[] = {1, 2, 3, 4, 5, 10};
	struct uk_thread *current = uk_thread_current();
	struct uk_thread *threads[FLEXOS_TEST_CONCURRENCY_MAX_THREADS];

	for (size_t i = 0; i < sizeof(thread_counts); ++i) {
		uint8_t thread_cnt = thread_counts[i];
		for (uint8_t j = 0; j < thread_cnt; ++j) {
			threads[j] = uk_sched_thread_create(current->sched, NULL, NULL, &thread_func_yield, NULL);
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
	uk_pr_info("yield test: SUCCESS\n");
}
