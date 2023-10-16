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

#if !LINUX_USERLAND
#include <flexos/microbenchmarks/isolated.h>
#include <flexos/isolation.h>
#include <uk/alloc.h>
#else
#include <inttypes.h>
#endif

#if CONFIG_LIBFLEXOS_VMEPT
#include <flexos/impl/main_annotation.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

// some config checks here...
#if !LINUX_USERLAND && CONFIG_LIBFLEXOS_INTELPKU && !CONFIG_LIBFLEXOS_GATE_INTELPKU_NO_INSTRUMENT
#error "Microbenchmarks should not be executed with gate instrumentation!"
#endif

// bench_start returns a timestamp for use to measure the start of a benchmark
// run.
//__attribute__ ((always_inline)) static inline uint64_t bench_start(void)
//{
//  unsigned  cycles_low, cycles_high;
//  asm volatile( "CPUID\n\t" // serialize
//                "RDTSC\n\t" // read clock
//                "MOV %%edx, %0\n\t"
//                "MOV %%eax, %1\n\t"
//                : "=r" (cycles_high), "=r" (cycles_low)
//                :: "%rax", "%rbx", "%rcx", "%rdx" );
//  return ((uint64_t) cycles_high << 32) | cycles_low;
//}

__attribute__ ((always_inline)) static inline uint64_t bench_start_morello(void)
{
  uint64_t val;
    asm volatile(
        "isb\n"
        "mrs %0, PMCCNTR_EL0" : "=r" (val));

    return val;
}

// bench_end returns a timestamp for use to measure the end of a benchmark run.
//__attribute__ ((always_inline)) static inline uint64_t bench_end(void)
//{
//  unsigned  cycles_low, cycles_high;
//  asm volatile( "RDTSCP\n\t" // read clock + serialize
//                "MOV %%edx, %0\n\t"
//                "MOV %%eax, %1\n\t"
//                "CPUID\n\t" // serialize -- but outside clock region!
//                : "=r" (cycles_high), "=r" (cycles_low)
//                :: "%rax", "%rbx", "%rcx", "%rdx" );
//  return ((uint64_t) cycles_high << 32) | cycles_low;
//}

__attribute__ ((always_inline)) static inline uint64_t bench_end_morello(void)
{
  uint64_t val;
    asm volatile(
        "isb\n"
        "mrs %0, PMCCNTR_EL0" : "=r" (val));

    return val;
}

__attribute__ ((always_inline)) static inline uint64_t l1d_cache_refill(void)
{
  uint64_t val;
    asm volatile(
        "isb\n"
        "mrs %0, pmevcntr0_el0" : "=r" (val));

    return val;
}

#if !LINUX_USERLAND
/*
 * make sure function does not get inlined
 */
__attribute__ ((noinline))
void empty_fcall(void) {
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_1xB(void) {
    char characters[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_1xBs(void) {
    char characters[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_2xB(void) {
    char characters1[1];
    char characters2[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_2xBs(void) {
    char characters1[1];
    char characters2[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_3xB(void) {
    char characters1[1];
    char characters2[1];
    char characters3[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_3xBs(void) {
    char characters1[1];
    char characters2[1];
    char characters3[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_4xB(void) {
    char characters1[1];
    char characters2[1];
    char characters3[1];
    char characters4[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}

__attribute__ ((noinline))
void empty_fcall_4xBs(void) {
    char characters1[1];
    char characters2[1];
    char characters3[1];
    char characters4[1];
    /* keep the call from being optimized away */
    asm volatile ("");
}
#endif

#define REPS 50

#define SERIAL 0

static inline void RUN_ISOLATED_FCALL(void)
{
#if !LINUX_USERLAND
	//flexos_nop_gate(0, 1, flexos_microbenchmarks_empty_fcall);
__flexos_morello_gate1_i(0,1,flexos_microbenchmarks_empty_fcall,0);
#else
	syscall(1000);
#endif
}

__attribute__ ((noinline)) void RUN_FCALL(void)
{
	asm volatile ("");
}

int main(int argc, char *argv[])
{
    uint64_t overhead_tsc, overhead_gate, overhead_fcall, t0, t1, l1d_end, l1d_start, l1d_total;
//    test_things();
    // uint64_t thing = 67;
    // uint64_t *__capability cap_to_pass;
    // cap_to_pass = cheri_ptr(&thing, sizeof(thing));
    // uint64_t ret_val = 0;
    // //ret_val = *cap_to_pass;
    // __flexos_morello_gate1_r(0,1, ret_val, flexos_microbenchmarks_return_int_cap_fcall,cap_to_pass);
    //uk_pr_crit("hello\n");
    //printf("result %d\n", ret_val);


#if !SERIAL
//    printf("> serial\n");
//    printf("TSC\tgate\tfcall\n");
    for(int i = 0; i < REPS; i++) {
        t0 = bench_start_morello();
        asm volatile("");
        t1 = bench_end_morello();
        overhead_tsc = t1 - t0;

        //t0 = bench_start_morello();
        //l1d_start = l1d_cache_refill();
	RUN_ISOLATED_FCALL();
        //l1d_end = l1d_cache_refill();
        //l1d_total = l1d_end-l1d_start;

        //t1 = bench_end_morello();
        //overhead_gate = t1 - t0;

//         t0 = bench_start_morello();
// 	//RUN_FCALL();
// //    getpid();
// //  __asm__ volatile (
// //           //  "ldr x26, [%0]\n"
// //             "ldp c26, c27, [%0]\n"
// //             "blr c27\n"
// //             :
// //             : "r"((uintptr_t *)(&(unsealed)))
// //             : "c29", "c26", "c27"
// //         );
//         t1 = bench_end_morello();
//         overhead_fcall = t1 - t0;

        // printf("%lld\t%lld\t%lld\n", overhead_tsc,
		// 			overhead_gate, overhead_fcall);

        printf("Timing 0: %d, Timing1: %d, Timing2: %d, Timing3: %d, Timing4: %d (unimportant), Timing5: %d, Timing6: %d, internal total: %d, total: %d, total 2: %d\n", cycles[7]-t0,cycles[1]-cycles[0], cycles[2]-cycles[1], cycles[3]-cycles[2], cycles[4]-cycles[3], cycles[5]-cycles[4], cycles[6]-cycles[5], cycles[6]-cycles[0], overhead_gate, cycles[0]-t0);
        printf("L1 cache refill events: %d\n", l1d_total);
    }
#else
    printf("\n#instruction,latency\n");
    uint64_t min = 1000, max = 0, sum = 0;
    int ok = 0;

    while (!ok) {
        for(int i = 0; i < REPS; i++) {
            t0 = bench_start_morello();
            asm volatile("");
            t1 = bench_end_morello();
            if ((t1 - t0) < min) { min = (t1 - t0); }
            if ((t1 - t0) > max) { max = (t1 - t0); }
    	    sum += (t1 - t0);
        }
	overhead_tsc = min;

        min = 1000, max = 0, sum = 0;
        for(int i = 0; i < REPS; i++) {
            t0 = bench_start_morello();
    	    RUN_FCALL();
            t1 = bench_end_morello();
            if ((t1 - t0) < min) { min = (t1 - t0); }
            if ((t1 - t0) > max) { max = (t1 - t0); }
    	    sum += (t1 - t0);
        }
	overhead_fcall = min;

	if (overhead_fcall > overhead_tsc) ok = 1;
    }

    printf("fcall,%" PRId64 "\n", overhead_fcall - overhead_tsc);

    min = 1000, max = 0, sum = 0;
    for(int i = 0; i < REPS; i++) {
        t0 = bench_start_morello();
	RUN_ISOLATED_FCALL();
        t1 = bench_end_morello();
        if ((t1 - t0) < min) { min = (t1 - t0); }
        if ((t1 - t0) > max) { max = (t1 - t0); }
	sum += (t1 - t0);
    }

    printf(
#if CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS && CONFIG_LIBFLEXOS_ENABLE_DSS
	"pku-dss,%"
#elif CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS
	"pku-heap,%"
#elif CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS
	"pku-shared,%"
#elif CONFIG_LIBFLEXOS_VMEPT
	"ept,%"
#elif LINUX_USERLAND
	"scall,%"
#else
	"gate,%"
#endif
    PRId64 "\n", min - overhead_tsc);
#endif

#define BENCH_NB(NB,p)					\
do {							\
    min = 1000, max = 0, sum = 0;			\
    for(int i = 0; i < REPS; i++) {			\
        t0 = bench_start_morello();				\
	empty_fcall_ ## NB ## xBs();			\
        t1 = bench_end_morello();				\
        if ((t1 - t0) < min) { min = (t1 - t0); }	\
        if ((t1 - t0) > max) { max = (t1 - t0); }	\
	sum += (t1 - t0);				\
    }							\
    overhead_gate = min - overhead_tsc;			\
							\
    printf(p);						\
    printf(STRINGIFY(NB) ",%" PRId64 "\n", overhead_gate);\
} while(0)

#if CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS
#if CONFIG_LIBFLEXOS_ENABLE_DSS
    char prefix[] = "dss";
    printf("\n\n#allocations,dss_latency\n");
#else
    char prefix[] = "heap";
    printf("\n\n#allocations,heap_latency\n");
#endif

    BENCH_NB(1, prefix);
    BENCH_NB(2, prefix);
    BENCH_NB(3, prefix);
    BENCH_NB(4, prefix);
#endif /* CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS */

    return 0;
}
