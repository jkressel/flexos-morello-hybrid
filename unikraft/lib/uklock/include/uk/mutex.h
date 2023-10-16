/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *
 * Copyright (c) 2018, NEC Europe Ltd., NEC Corporation. All rights reserved.
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

#ifndef __UK_MUTEX_H__
#define __UK_MUTEX_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

#include <uk/config.h>
#include <flexos/isolation.h>

#if CONFIG_LIBUKLOCK_MUTEX
#include <uk/assert.h>
#include <uk/plat/lcpu.h>
#include <uk/thread.h>
#include <uk/wait.h>
#include <uk/wait_types.h>
#include <uk/plat/time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Mutex that relies on a scheduler
 * uses wait queues for threads
 */
struct uk_mutex {
	int lock_count;
	struct uk_thread *owner;
	struct uk_waitq wait;
};

#define	UK_MUTEX_INITIALIZER(name)				\
	{ 0, NULL, __WAIT_QUEUE_INITIALIZER((name).wait) }

void uk_mutex_init(struct uk_mutex *m);

static inline void uk_mutex_lock(struct uk_mutex *m)
{
	struct uk_thread *current;
	unsigned long irqf;

	/* Volatile to make sure that the compiler doesn't reorganize
	 * the code in such a way that the dereference happens in the
	 * other domain... */
	volatile struct uk_waitq *wq = &m->wait;
	volatile int lock_count = m->lock_count;
	volatile struct uk_thread *owner = m->owner;

	UK_ASSERT(m);

	flexos_nop_gate_r(0, 0, current, uk_thread_current);

	for (;;) {
	do {
		struct uk_thread *__current;
		unsigned long flags;
		DEFINE_WAIT(__wait);
		if (lock_count == 0 || owner == current)
			break;
		for (;;) {
			__current = uk_thread_current();
				/* protect the list */
			flags = ukplat_lcpu_save_irqf();
			uk_waitq_add(wq, __wait);
			flexos_nop_gate(0, 0, uk_thread_set_wakeup_time,
					__current, 0);
			flexos_nop_gate(0, 0, clear_runnable, __current);
			flexos_nop_gate(0, 0, uk_sched_thread_blocked,
					__current);
			ukplat_lcpu_restore_irqf(flags);
			if (lock_count == 0 || owner == current)
				break;
			flexos_nop_gate(0, 0, uk_sched_yield);
		}
		flags = ukplat_lcpu_save_irqf();
			/* need to wake up */
		flexos_nop_gate(0, 0, uk_thread_wake, __current);
		uk_waitq_remove(wq, __wait);
		ukplat_lcpu_restore_irqf(flags);
	} while (0);

	irqf = ukplat_lcpu_save_irqf();
	if (m->lock_count == 0 || m->owner == current)
		break;
	ukplat_lcpu_restore_irqf(irqf);
}
	m->lock_count++;
	m->owner = current;
	ukplat_lcpu_restore_irqf(irqf);

	//instrument-gate
}

static inline int uk_mutex_trylock(struct uk_mutex *m)
{
	struct uk_thread *current;
	unsigned long irqf;
	int ret = 0;

	UK_ASSERT(m);

	flexos_nop_gate_r(0, 0, current, uk_thread_current);

	irqf = ukplat_lcpu_save_irqf();
	if (m->lock_count == 0 || m->owner == current) {
		ret = 1;
		m->lock_count++;
		m->owner = current;
	}
	ukplat_lcpu_restore_irqf(irqf);
	return ret;
}

static inline int uk_mutex_is_locked(struct uk_mutex *m)
{
	return m->lock_count > 0;
}

static inline void uk_mutex_unlock(struct uk_mutex *m)
{
	unsigned long irqf;
	/* regarding volatile, see previous comment */
	volatile struct uk_waitq *wq = &m->wait;

	UK_ASSERT(m);

	irqf = ukplat_lcpu_save_irqf();
	UK_ASSERT(m->lock_count > 0);
	if (--m->lock_count == 0) {
		m->owner = NULL;
		flexos_nop_gate(0, 0, uk_waitq_wake_up, wq);
	}
	ukplat_lcpu_restore_irqf(irqf);

	//instrument-gate
}

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_LIBUKLOCK_MUTEX */

#pragma GCC diagnostic pop

#endif /* __UK_MUTEX_H__ */
