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

#ifndef LIBFLEXOSEXAMPLE_H
#define LIBFLEXOSEXAMPLE_H

#define FLEXOS_TEST_CONCURRENCY_BUF_SIZE 128

/* The main thread has tid 0 and the RPC server has tid 1, so
 * the worker threads will have tids starting from 2. */
#define FLEXOS_TEST_CONCURRENCY_TID_BIAS 2

#define FLEXOS_TEST_CONCURRENCY_MAX_THREADS 10

int   function1(char *buf);
int   function11();
char *function2(void);
int call_isolated_func1(char* buffer);

int ping1(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
int ping2(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
int ping3(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
int ping4(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
int ping5(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
int ping6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
void reset_runs(void);

void write_to_buf(size_t buf_index, size_t i, char byte);
char read_from_buf(size_t buf_index, size_t i);

unsigned int fib1(unsigned int n);

void lib_test_start();
int is_main_thread_exited();

#endif /* LIBFLEXOSEXAMPLE_H */
