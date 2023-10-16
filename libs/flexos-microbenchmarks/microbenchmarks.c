/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2021, Hugo Lefeuvre <hugo.lefeuvre@manchester.ac.uk>
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

#include <flexos/microbenchmarks/isolated.h>

/*
 * make sure function does not get inlined
 */
__attribute__ ((noinline)) void flexos_microbenchmarks_empty_fcall(void) {
    /* keep the call from being optimized away */
    asm volatile ("");
}

/*
 * make sure function does not get inlined
 */
// int flexos_microbenchmarks_return_int_fcall(int* val, char *val2, int val3) {
//     /* keep the call from being optimized away */
//     asm volatile ("");
//     return 0;
// }


// int another_function_to_test(uint64_t* val3) {
//     /* keep the call from being optimized away */
//     asm volatile ("");
//     return 0;
// }

// int* another_function_to_test1(int val3) {
//     /* keep the call from being optimized away */
//     asm volatile ("");
//     return 0;
// }

// uintptr_t* another_function_to_test1(uint64_t* val3) {
//     /* keep the call from being optimized away */
//     asm volatile ("");
//     return 0;
// }

// void something(int *__capability ptr) {
//      *ptr = 12345;
//     // int* good_ptr = ptr;
//     // int *other_good_ptr;
//     // other_good_ptr = ptr;
// }
 
// __attribute__ ((noinline)) int something1p(int* ptr) {
//    //  printf("thing %d\n", *ptr);
//      return *ptr+3;
//     // int* good_ptr = ptr;
//     // int *other_good_ptr;
//     // other_good_ptr = ptr;
// }

// __attribute__ ((noinline)) int something1c(int *__capability ptr) {
//    //  printf("thing %d\n", *ptr);
//      return *ptr+3;
//     // int* good_ptr = ptr;
//     // int *other_good_ptr;
//     // other_good_ptr = ptr;
// }

// #define something1(a) _Generic(a, uint64_t *__capability: something1c, int*: something1p)(a)


// uint64_t flexos_microbenchmarks_return_int_cap_fcall(__capability uint64_t * val) {
//     //something(val);
//     int value = * val;
//     value += 100;
//     int arrs[100];
//     uint64_t *__capability val1 = val;
//     //printf("result %d\n", value);
//     //int *__capability thinge = &value;
//     value += something1(val1);
//     value += something1(&value);
//     return value;
//     // int *__capability thing[29];
//     // thing[23] = 0x77;
//     // something((int *__capability) &value);
//     // return (int)(arrs[0]);
// }
