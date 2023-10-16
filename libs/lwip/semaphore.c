/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2019, NEC Laboratories Europe GmbH, NEC Corporation.
 *                     All rights reserved.
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
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#include <uk/semaphore.h>
#include <uk/arch/time.h>
#include <lwip/sys.h>
#include <flexos/isolation.h>

#include <uk/essentials.h>

/**
 * Initializes a new semaphore. The "count" argument specifies
 * the initial state of the semaphore.
 */
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
	flexos_gate(libuklock, uk_semaphore_init, &sem->sem, (long) count);
	sem->valid = 1;
	return ERR_OK;
}

int sys_sem_valid(sys_sem_t *sem)
{
	return (sem->valid == 1);
}

void sys_sem_set_invalid(sys_sem_t *sem)
{
	sem->valid = 0;
}

void sys_sem_free(sys_sem_t *sem)
{
	sys_sem_set_invalid(sem);
}

/* Signals a semaphore. */
void sys_sem_signal(sys_sem_t *sem)
{
	flexos_gate(uklock, uk_semaphore_up, &sem->sem);
}

/* NOTE FLEXOS: for simplicity purposes we execute this in uklock's domain,
 * but this should probably done with individual gates if uklock is isolated.
 */
static inline __nsec sys_arch_sem_wait_helper(volatile struct uk_semaphore *s) {
	__nsec nsret = ukplat_monotonic_clock();
	uk_semaphore_down(s);
	return ukplat_monotonic_clock() - nsret;
}

/**
 * Blocks the thread while waiting for the semaphore to be
 * signaled. If the "timeout" argument is non-zero, the thread should
 * only be blocked for the specified time (measured in
 * milliseconds).
 *
 * If the timeout argument is non-zero, the return value is the number of
 * milliseconds spent waiting for the semaphore to be signaled. If the
 * semaphore wasn't signaled within the specified time, the return value is
 * SYS_ARCH_TIMEOUT. If the thread didn't have to wait for the semaphore
 * (i.e., it was already signaled), the function may return zero.
 */
#if CONFIG_LIBFLEXOS_INTELPKU
#pragma GCC push_options
#pragma GCC optimize("O0")
#endif
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
	__nsec nsret;
	volatile struct uk_semaphore *s = &((volatile sys_sem_t *) sem)->sem;

	flexos_gate(ukdebug, uk_pr_debug, FLEXOS_SHARED_LITERAL("sys_arch_sem_wait(%p, %lu)\n"), sem, timeout);
	if (timeout == 0) {
		flexos_gate_r(uklock, nsret, sys_arch_sem_wait_helper, s);
	} else {
		flexos_gate_r(uklock, nsret, uk_semaphore_down_to, s,
					ukarch_time_msec_to_nsec((__nsec) timeout));
		if (unlikely(nsret == __NSEC_MAX))
			return SYS_ARCH_TIMEOUT;
	}

	return (u32_t) ukarch_time_nsec_to_msec(nsret);
}
#if CONFIG_LIBFLEXOS_INTELPKU
#pragma GCC pop_options
#endif
