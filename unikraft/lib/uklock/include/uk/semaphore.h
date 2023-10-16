/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/* Taken and modified from Mini-OS (include/semphore.h) */
#ifndef __UK_SEMAPHORE_H__
#define __UK_SEMAPHORE_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

#include <uk/config.h>
#include <flexos/isolation.h>

#if CONFIG_LIBUKLOCK_SEMAPHORE
#include <uk/print.h>
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
 * Semaphore that relies on a scheduler
 * uses wait queues for threads
 */
struct uk_semaphore {
	long count;
	struct uk_waitq wait;
};

void uk_semaphore_init(struct uk_semaphore *s, long count);

static inline void uk_semaphore_down(struct uk_semaphore *s)
{
	unsigned long irqf;

	UK_ASSERT(s);

	for (;;) {
		do {
			struct uk_thread *__current;
			unsigned long flags;
			DEFINE_WAIT(__wait);
			if (s->count > 0)
				break;
			for (;;) {
				__current = uk_thread_current();
				/* protect the list */
				flags = ukplat_lcpu_save_irqf();
				uk_waitq_add(&s->wait, __wait);
				flexos_nop_gate(0, 0,
						uk_thread_set_wakeup_time,
						__current, 0);
				flexos_nop_gate(0, 0, clear_runnable,
						__current);
				flexos_nop_gate(0, 0, uk_sched_thread_blocked,
						__current);
				ukplat_lcpu_restore_irqf(flags);
				if (s->count > 0)
					break;
				flexos_nop_gate(0, 0, uk_sched_yield);
			}
			flags = ukplat_lcpu_save_irqf();
			/* need to wake up */
			flexos_nop_gate(0, 0, uk_thread_wake, __current);
			uk_waitq_remove(&s->wait, __wait);
			ukplat_lcpu_restore_irqf(flags);
		} while (0);

		irqf = ukplat_lcpu_save_irqf();
		if (s->count > 0)
			break;
		ukplat_lcpu_restore_irqf(irqf);
	}
	--s->count;
#ifdef UK_SEMAPHORE_DEBUG
	uk_pr_debug("Decreased semaphore %p to %ld\n", s, s->count);
#endif
	ukplat_lcpu_restore_irqf(irqf);
}

static inline int uk_semaphore_down_try(struct uk_semaphore *s)
{
	unsigned long irqf;
	int ret = 0;

	UK_ASSERT(s);

	irqf = ukplat_lcpu_save_irqf();
	if (s->count > 0) {
		ret = 1;
		--s->count;
#ifdef UK_SEMAPHORE_DEBUG
		uk_pr_debug("Decreased semaphore %p to %ld\n",
			    s, s->count);
#endif
	}
	ukplat_lcpu_restore_irqf(irqf);
	return ret;
}

/* Returns __NSEC_MAX on timeout, expired time when down was successful */
static inline __nsec uk_semaphore_down_to(struct uk_semaphore *s,
					  __nsec timeout)
{
	unsigned long irqf;
	__nsec then = ukplat_monotonic_clock();
	__nsec deadline;

	UK_ASSERT(s);

	deadline = then + timeout;

	for (;;) {
		do {
			struct uk_thread *__current;
			unsigned long flags;
			DEFINE_WAIT(__wait);
			if (s->count > 0)
				break;
			for (;;) {
				__current = uk_thread_current();
				/* protect the list */
				flags = ukplat_lcpu_save_irqf();
				uk_waitq_add(&s->wait, __wait);
				flexos_nop_gate(0, 0,
						uk_thread_set_wakeup_time,
						__current, deadline);
				flexos_nop_gate(0, 0, clear_runnable,
						__current);
				flexos_nop_gate(0, 0, uk_sched_thread_blocked,
						__current);
				ukplat_lcpu_restore_irqf(flags);
				if (s->count > 0 || (deadline && ukplat_monotonic_clock() >= deadline))
					break;
				flexos_nop_gate(0, 0, uk_sched_yield);
			}
			flags = ukplat_lcpu_save_irqf();
			/* need to wake up */
			flexos_nop_gate(0, 0, uk_thread_wake, __current);
			uk_waitq_remove(&s->wait, __wait);
			ukplat_lcpu_restore_irqf(flags);
		} while (0);

		irqf = ukplat_lcpu_save_irqf();
		if (s->count > 0 || (deadline &&
				     ukplat_monotonic_clock() >= deadline))
			break;
		ukplat_lcpu_restore_irqf(irqf);
	}

	if (s->count > 0) {
		s->count--;
#ifdef UK_SEMAPHORE_DEBUG
		uk_pr_debug("Decreased semaphore %p to %ld\n",
			    s, s->count);
#endif
		ukplat_lcpu_restore_irqf(irqf);
		return ukplat_monotonic_clock() - then;
	}

	ukplat_lcpu_restore_irqf(irqf);
#ifdef UK_SEMAPHORE_DEBUG
	uk_pr_debug("Timed out while waiting for semaphore %p\n", s);
#endif
	return __NSEC_MAX;
}

static inline void uk_semaphore_up(struct uk_semaphore *s)
{
	unsigned long irqf;

	UK_ASSERT(s);

	irqf = ukplat_lcpu_save_irqf();
	++s->count;
#ifdef UK_SEMAPHORE_DEBUG
	uk_pr_debug("Increased semaphore %p to %ld\n",
		    s, s->count);
#endif
	/* Volatile to make sure that the compiler doesn't reorganize
	 * the code in such a way that the dereference happens in the
	 * other domain... */
	volatile struct uk_waitq *wq = &s->wait;
	flexos_nop_gate(0, 0, uk_waitq_wake_up, wq);
	ukplat_lcpu_restore_irqf(irqf);
}

#ifdef __cplusplus
}
#endif

#endif /* CONFIG_LIBUKLOCK_SEMAPHORE */

#pragma GCC diagnostic pop

#endif /* __UK_SEMAPHORE_H__ */
