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
/*
 * Thread definitions
 * Ported from Mini-OS
 */
#ifndef __UK_THREAD_H__
#define __UK_THREAD_H__

#include <flexos/isolation.h>
#include <stdint.h>
#include <stdbool.h>
#ifdef CONFIG_LIBNEWLIBC
#include <sys/reent.h>
#endif
#include <uk/arch/lcpu.h>
#include <uk/arch/time.h>
#include <uk/plat/thread.h>
#if CONFIG_LIBUKSIGNAL
#include <uk/uk_signal.h>
#endif
#include <uk/thread_attr.h>
#include <uk/wait_types.h>
#include <uk/list.h>
#include <uk/page.h>
#include <uk/essentials.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uk_sched;
struct uk_sched *uk_sched_get_default(void);
struct uk_thread *uk_sched_thread_create(struct uk_sched *sched,
		const char *name, const uk_thread_attr_t *attr,
		void (*function)(void *), void *arg);

struct uk_thread {
	const char *name;
	void *stack;
	void *tls;
	void *ctx;
	UK_TAILQ_ENTRY(struct uk_thread) thread_list;
	uint32_t flags;
	__snsec wakeup_time;
	bool detached;
	struct uk_waitq waiting_threads;
	struct uk_sched *sched;
	void *prv;
#if CONFIG_LIBFLEXOS_INTELPKU || CONFIG_LIBFLEXOS_MORELLO
	int tid;
#endif /* CONFIG_LIBFLEXOS_INTELPKU */
#if CONFIG_LIBFLEXOS_VMEPT
	/* a tid in [0, 255] indicates normal thread
	 * a tid of -1 indicates rpc thread */
	int tid;
	/* used to identify the rpc control data this rpc thread listens to */
	void* ctrl;
#endif /* CONFIG_LIBFLEXOS_VMEPT */
#ifdef CONFIG_LIBNEWLIBC
	struct _reent *reent;
#endif
#if CONFIG_LIBUKSIGNAL
	struct uk_thread_sig *signals_container;
#endif
};

#if CONFIG_LIBFLEXOS_INTELPKU
#include <flexos/impl/intelpku.h>
#endif /* CONFIG_LIBFLEXOS_INTELPKU */

UK_TAILQ_HEAD(uk_thread_list, struct uk_thread);

#define uk_thread_create_attr_main(attr, function, data) \
	uk_sched_thread_create_main(uk_sched_get_default(), \
			attr, function, data)
#define uk_thread_create_main(function, data) \
	uk_thread_create_attr_main(NULL, function, data)

static inline struct uk_thread *uk_thread_create_attr(const char *name,
	const uk_thread_attr_t *attr,void (*function)(void *), void *arg)
{
	return uk_sched_thread_create(uk_sched_get_default(),
			name, attr, function, arg);
}

static inline struct uk_thread *uk_thread_create(const char *name,
	void (*function)(void *), void *arg)
{
	return uk_thread_create_attr(name, NULL, function, arg);
}

void uk_sched_thread_kill(struct uk_sched *sched,
		struct uk_thread *thread);

static inline void uk_thread_kill(struct uk_thread *thread)
{
	uk_sched_thread_kill(thread->sched, thread);
}

static inline void uk_thread_set_wakeup_time(struct uk_thread *thread, __snsec time)
{
	thread->wakeup_time = time;
}

void uk_thread_exit(struct uk_thread *thread);

int uk_thread_wait(struct uk_thread *thread);
int uk_thread_detach(struct uk_thread *thread);

int uk_thread_set_prio(struct uk_thread *thread, prio_t prio);
int uk_thread_get_prio(const struct uk_thread *thread, prio_t *prio);

int uk_thread_set_timeslice(struct uk_thread *thread, int timeslice);
int uk_thread_get_timeslice(const struct uk_thread *thread, int *timeslice);

static inline
void *uk_thread_get_private(struct uk_thread *thread)
{
	return thread->prv;
}

static inline
void uk_thread_set_private(struct uk_thread *thread, void *prv)
{
	thread->prv = prv;
}

static inline
struct uk_thread *uk_thread_current(void)
{
	struct uk_thread **current;
	unsigned long sp = ukarch_read_sp();

	current = (struct uk_thread **) (sp & STACK_MASK_TOP);

	return *current;
}

static inline
struct uk_thread_sig *uk_crr_thread_sig_container(void)
{
	return uk_thread_current()->signals_container;
}

void uk_thread_inherit_signal_mask(struct uk_thread *thread);

#define RUNNABLE_FLAG   0x00000001
#define EXITED_FLAG     0x00000002
#define QUEUEABLE_FLAG  0x00000004

#define is_runnable(_thread)    ((_thread)->flags &   RUNNABLE_FLAG)
#define set_runnable(_thread)   ((_thread)->flags |=  RUNNABLE_FLAG)

static inline void clear_runnable(struct uk_thread *thread)
{
	thread->flags &= ~RUNNABLE_FLAG;
}

#define is_exited(_thread)      ((_thread)->flags &   EXITED_FLAG)
#define set_exited(_thread)     ((_thread)->flags |=  EXITED_FLAG)

#define is_queueable(_thread)    ((_thread)->flags &   QUEUEABLE_FLAG)
#define set_queueable(_thread)   ((_thread)->flags |=  QUEUEABLE_FLAG)
#define clear_queueable(_thread) ((_thread)->flags &= ~QUEUEABLE_FLAG)

int uk_thread_init_idle(struct uk_thread *thread,
		struct ukplat_ctx_callbacks *cbs, struct uk_alloc *allocator,
		const char *name, void *stack
		, void* stack_comp1, void* stack_comp2
,
		void *tls, void (*function)(void *), void *arg);
int uk_thread_init_main(struct uk_thread *thread,
		struct ukplat_ctx_callbacks *cbs, struct uk_alloc *allocator,
		const char *name, void *stack
		, void* stack_comp1, void* stack_comp2
,
		void *tls, void (*function)(void *), void *arg);
int uk_thread_init(struct uk_thread *thread,
		struct ukplat_ctx_callbacks *cbs, struct uk_alloc *allocator,
		const char *name, void *stack
		, void* stack_comp1, void* stack_comp2
,
		void *tls, void (*function)(void *), void *arg);
void uk_thread_fini(struct uk_thread *thread,
		struct uk_alloc *allocator);
void uk_thread_block_timeout(struct uk_thread *thread, __nsec nsec);
void uk_thread_block(struct uk_thread *thread);
void uk_thread_wake(struct uk_thread *thread);

#ifdef __cplusplus
}
#endif

#endif /* __UK_THREAD_H__ */
