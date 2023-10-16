/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Costin Lupu <costin.lupu@cs.pub.ro>
 *
 * Copyright (c) 2017, NEC Europe Ltd., NEC Corporation. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include <uk/plat/config.h>
#include <uk/plat/thread.h>
#include <flexos/isolation.h>
#include <uk/alloc.h>
#include <uk/sched.h>
#include <uk/arch/tls.h>
#if CONFIG_LIBUKSCHEDCOOP
#include <uk/schedcoop.h>
#endif
#if CONFIG_LIBUKSIGNAL
#include <uk/uk_signal.h>
#endif

struct uk_sched *uk_sched_head;

/* TODO FLEXOS: for now we share the TLS. This is not optimal
 * from a security stand point, and should be revisited with a
 * more thoughtful approach. */

/* FIXME Support for external schedulers */
struct uk_sched *uk_sched_default_init(struct uk_alloc *a)
{
	struct uk_sched *s = NULL;

#if CONFIG_LIBUKSIGNAL
	uk_proc_sig_init(&uk_proc_sig);
#endif

#if CONFIG_LIBUKSCHEDCOOP
	s = uk_schedcoop_init(a);
#endif

	return s;
}

int uk_sched_register(struct uk_sched *s)
{
	struct uk_sched *this = uk_sched_head;

	if (!uk_sched_head) {
		uk_sched_head = s;
		s->next = NULL;
		return 0;
	}

	while (this && this->next)
		this = this->next;
	this->next = s;
	s->next = NULL;
	return 0;
}

struct uk_sched *uk_sched_get_default(void)
{
	return uk_sched_head;
}

int uk_sched_set_default(struct uk_sched *s)
{
	struct uk_sched *head, *this, *prev;

	head = uk_sched_get_default();

	if (s == head)
		return 0;

	if (!head) {
		uk_sched_head = s;
		return 0;
	}

	this = head;
	while (this->next) {
		prev = this;
		this = this->next;
		if (s == this) {
			prev->next = this->next;
			this->next = head->next;
			head = this;
			return 0;
		}
	}

	/* s is not registered yet. Add in front of the queue. */
	s->next = head;
	uk_sched_head = s;
	return 0;
}

struct uk_sched *uk_sched_create(struct uk_alloc *a, size_t prv_size)
{
	struct uk_sched *sched = NULL;

	UK_ASSERT(a != NULL);

	sched = uk_malloc(a, sizeof(struct uk_sched) + prv_size);
	if (sched == NULL) {
		flexos_nop_gate(0, 0, uk_pr_warn,
				"Failed to allocate scheduler\n");
		return NULL;
	}

	sched->threads_started = false;
	sched->allocator = a;
	UK_TAILQ_INIT(&sched->exited_threads);
	sched->prv = (void *) sched + sizeof(struct uk_sched);

	return sched;
}

void uk_sched_start(struct uk_sched *sched)
{
	UK_ASSERT(sched != NULL);
	ukplat_thread_ctx_start(&sched->plat_ctx_cbs, sched->idle.ctx);
}

static void *create_stack(struct uk_alloc *allocator)
{
	void *stack;

	if (uk_posix_memalign(allocator, &stack,
	/* TODO FLEXOS for some reason with DSS the allocation always fails
	 * with the buddy allocator, commenting this should be fine though. */
#if 0 && CONFIG_LIBFLEXOS_ENABLE_DSS
	/* if the DSS is enabled, allocate two times the size of the
	 * stack; the second half is then used as data shadow stack */
			      STACK_SIZE, STACK_SIZE * 2) != 0) {
#else
			      STACK_SIZE, STACK_SIZE) != 0) {
#endif /* CONFIG_LIBFLEXOS_ENABLE_DSS */
		flexos_nop_gate(0, 0, uk_pr_err,
				FLEXOS_SHARED_LITERAL("Failed to allocate thread stack: Not enough memory\n"));
		return NULL;
	}

#if CONFIG_LIBFLEXOS_GATE_INTELPKU_SHARED_STACKS
	flexos_intelpku_mem_set_key(stack, STACK_SIZE / __PAGE_SIZE, 15);
#endif /* CONFIG_LIBFLEXOS_INTELPKU */

	return stack;
}

static void *uk_thread_tls_create(struct uk_alloc *allocator)
{
	void *tls;

	if (uk_posix_memalign(allocator, &tls, ukarch_tls_area_align(),
			      ukarch_tls_area_size()) != 0) {
		flexos_nop_gate(0, 0, uk_pr_err,
				"Failed to allocate thread TLS area\n");
		return NULL;
	}
	ukarch_tls_area_copy(tls);
	return tls;
}

#if CONFIG_LIBFLEXOS_INTELPKU

static inline void PROTECT_STACK(void *stack, int key)
{
	/* FIXME FLEXOS: hack to support boot time 0x0 domain */
	if (rdpkru() == 0x0) {
		flexos_intelpku_mem_set_key(stack, STACK_SIZE / __PAGE_SIZE, key);
	} else {
		flexos_nop_gate(0, 0, flexos_intelpku_mem_set_key, stack,
				STACK_SIZE / __PAGE_SIZE, key);
	}
}

#if CONFIG_LIBFLEXOS_ENABLE_DSS

/* DSSs are always shared */
#define SHARE_DSS(stack_comp)						\
	PROTECT_STACK((stack_comp) + STACK_SIZE, 15);

#else /* CONFIG_LIBFLEXOS_ENABLE_DSS */

#define SHARE_DSS(stack_comp)

#endif /* CONFIG_LIBFLEXOS_ENABLE_DSS */

#else /* CONFIG_LIBFLEXOS_INTELPKU */

#define SHARE_DSS(stack_comp)
#define PROTECT_STACK(stack_comp, key)

#endif /* CONFIG_LIBFLEXOS_INTELPKU */

#if CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS
#define COMP0_PKUKEY 0
#else
#define COMP0_PKUKEY 15
#endif /* CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS */

#define ALLOC_COMP_STACK(stack_comp, key) 				\
do {									\
	/* allocate stack for compartment 'key' */			\
	if ((stack_comp) == NULL)					\
		(stack_comp) = create_stack(sched->allocator);		\
	if ((stack_comp) == NULL)					\
		goto err;						\
	PROTECT_STACK((stack_comp), (key));				\
	SHARE_DSS((stack_comp));					\
} while (0)

#define ALLOC_COMP_STACK_MORELLO(stack_comp, allocator) 				\
do {									\
	/* allocate stack for compartment 'key' */			\
	if ((stack_comp) == NULL)					\
		(stack_comp) = create_stack(allocator);		\
	if ((stack_comp) == NULL)					\
		goto err;						\
} while (0)

void uk_sched_idle_init(struct uk_sched *sched,
		void *stack, void (*function)(void *))
{
	struct uk_thread *idle;
	int rc;
	void *tls = NULL;

	UK_ASSERT(sched != NULL);

	ALLOC_COMP_STACK_MORELLO(stack, comp0_allocator);

	// ALLOC_COMP_STACK(stack, COMP0_PKUKEY);

	void *stack_comp1 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp1, comp1_allocator);

void *stack_comp2 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp2, comp2_allocator);

//void *shared_stack = NULL;
//ALLOC_COMP_STACK_MORELLO(shared_stack, flexos_shared_alloc);


	if (have_tls_area() && !(tls = uk_thread_tls_create(flexos_shared_alloc)))
		goto err;

	idle = &sched->idle;

	/* same as main, we want to call the variant that doesn't execute gates */
	rc = uk_thread_init_main(idle,
			&sched->plat_ctx_cbs, sched->allocator,
			"Idle", stack , stack_comp1, stack_comp2

,
			tls, function, NULL);

	if (rc)
		goto err;

	idle->sched = sched;
	return;

err:
	UK_CRASH("Failed to initialize `idle` thread\n");
}

/* This copy of uk_sched_thread_create is used only for the creation of the
 * main thread. At that time we are still in the allmighty 0x0 domain,
 * meaning that gate wrappers are going to screw everything up.
 *
 * tl;dr this is uk_sched_thread_create without gates.
 */
struct uk_thread *uk_sched_thread_create_main(struct uk_sched *sched,
		const uk_thread_attr_t *attr,
		void (*function)(void *), void *arg)
{
	struct uk_thread *thread = NULL;
	void *stack = NULL;
	void *stack_1 = NULL;
	int rc;
	void *tls = NULL;

	thread = uk_malloc(sched->allocator, sizeof(struct uk_thread));
	if (thread == NULL) {
		uk_pr_err("Failed to allocate thread\n");
		goto err;
	}
//////////////////// HERE TODO Morello
	ALLOC_COMP_STACK_MORELLO(stack, comp0_allocator);

//ALLOC_COMP_STACK(stack_comp1, 1);

	void *stack_comp1 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp1, comp1_allocator);

	void *stack_comp2 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp2, comp2_allocator);

//void *stack_shared = NULL;
//ALLOC_COMP_STACK_MORELLO(stack_shared, flexos_shared_alloc);

	if (have_tls_area() && !(tls = uk_thread_tls_create(flexos_shared_alloc)))
		goto err;

	rc = uk_thread_init_main(thread,
			&sched->plat_ctx_cbs, sched->allocator,
			"main", stack , stack_comp1, stack_comp2

,
			tls, function, arg);
	if (rc)
		goto err;

	rc = uk_sched_thread_add(sched, thread, attr);
	if (rc)
		goto err_add;

	return thread;

err_add:
	uk_thread_fini(thread, sched->allocator);
err:
	if (tls)
		uk_free(flexos_shared_alloc, tls);
	if (stack)
		uk_free(sched->allocator, stack);
#if CONFIG_LIBFLEXOS_INTELPKU
	/* TODO FLEXOS free() per-compartment stacks */
	/* Clearly, not doing it now should not be much of an issue because
	 * this error case is unlikely to happen in our benchmarks... */
#endif /* CONFIG_LIBFLEXOS_INTELPKU */
	if (thread)
		uk_free(sched->allocator, thread);

	return NULL;
}

struct uk_thread *uk_sched_thread_create(struct uk_sched *sched,
		const char *name, const uk_thread_attr_t *attr,
		void (*function)(void *), void *arg)
{
	struct uk_thread *thread = NULL;
	void *stack = NULL;
	int rc;
	void *tls = NULL;

	thread = uk_malloc(sched->allocator, sizeof(struct uk_thread));
	if (thread == NULL) {
		flexos_nop_gate(0, 0, uk_pr_err,
				"Failed to allocate thread\n");
		goto err;
	}

//	ALLOC_COMP_STACK(stack, COMP0_PKUKEY);
	ALLOC_COMP_STACK_MORELLO(stack, comp0_allocator);

	void *stack_comp1 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp1, comp1_allocator);

	void *stack_comp2 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp2, comp2_allocator);

//void *shared_stack = NULL;
//ALLOC_COMP_STACK_MORELLO(shared_stack, flexos_shared_alloc);

	if (have_tls_area() && !(tls = uk_thread_tls_create(flexos_shared_alloc)))
		goto err;

	rc = uk_thread_init(thread,
			&sched->plat_ctx_cbs, sched->allocator,
			name, stack , stack_comp1, stack_comp2

,
			tls, function, arg);
	if (rc)
		goto err;

#if CONFIG_LIBFLEXOS_VMEPT
	/* here we need to create an rpc thread in each other compartment */
	// TODO: error handling
	printf("Spawning rpc threads in other compartments.\n");
	volatile struct flexos_vmept_rpc_ctrl * ctrl = flexos_vmept_rpc_ctrl(flexos_vmept_comp_id, thread->tid);
	flexos_vmept_init_rpc_ctrl(ctrl);
	thread->ctrl = ctrl;
	for (size_t i = 0; i < FLEXOS_VMEPT_COMP_COUNT; ++i) {
		if (i == flexos_vmept_comp_id)
			continue;
		flexos_vmept_master_rpc_call_create(flexos_vmept_comp_id, i, thread->tid);
	}
	printf("Spawned rpc threads in other compartments.\n");
#endif /* CONFIG_LIBFLEXOS_VMEPT */

	rc = uk_sched_thread_add(sched, thread, attr);
	if (rc)
		goto err_add;

	return thread;

err_add:
	uk_thread_fini(thread, sched->allocator);
err:
	if (tls)
		uk_free(flexos_shared_alloc, tls);
	if (stack)
		uk_free(sched->allocator, stack);
#if CONFIG_LIBFLEXOS_INTELPKU
	/* TODO FLEXOS free() per-compartment stacks */
	/* Clearly, not doing it now should not be much of an issue because
	 * this error case is unlikely to happen in our benchmarks... */
#endif /* CONFIG_LIBFLEXOS_INTELPKU */
	if (thread)
		uk_free(sched->allocator, thread);

	return NULL;
}

#if CONFIG_LIBFLEXOS_VMEPT
struct uk_thread *uk_sched_thread_create_rpc_only(struct uk_sched *sched,
		const char *name, const uk_thread_attr_t *attr,
		void (*function)(void *), void *arg,
		uint8_t normal_thread_comp_id, uint8_t normal_thread_tid,
		volatile struct flexos_vmept_thread_map *thread_map)
{
	volatile struct uk_thread *thread = NULL;
	void *stack = NULL;
	int rc;
	void *tls = NULL;

	thread = uk_malloc(sched->allocator, sizeof(struct uk_thread));
	if (thread == NULL) {
		flexos_nop_gate(0, 0, uk_pr_err,
				"Failed to allocate thread\n");
		goto err;
	}

	ALLOC_COMP_STACK(stack, COMP0_PKUKEY);

	void *stack_comp1 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp1, comp1_allocator);

	void *stack_comp2 = NULL;
ALLOC_COMP_STACK_MORELLO(stack_comp2, comp2_allocator);

//void *shared_stack = NULL;
//ALLOC_COMP_STACK_MORELLO(shared_stack, flexos_shared_alloc);

	if (have_tls_area() && !(tls = uk_thread_tls_create(flexos_shared_alloc)))
		goto err;

	rc = uk_thread_init(thread,
			&sched->plat_ctx_cbs, sched->allocator,
			name, stack , stack_comp1, stack_comp2

,
			tls, function, arg);
	if (rc)
		goto err;

	// for rpc only threads set tid to -1
	// FIXME: maybe change with proper tid allocation?
	thread->tid = -1;
	/* thread_map = NULL is used when creating the thread for the master rpc loop
	 * we don't set ctrl or the mapping for that thread */
	if (thread_map) {
		thread->ctrl = flexos_vmept_rpc_ctrl(normal_thread_comp_id, normal_thread_tid);
		flexos_vmept_thread_map_put(thread_map, normal_thread_comp_id,
			 (uint8_t) normal_thread_tid, thread);
	}
	rc = uk_sched_thread_add(sched, thread, attr);
	if (rc)
		goto err_add;

	return thread;

err_add:
	uk_thread_fini(thread, sched->allocator);
err:
	if (tls)
		uk_free(flexos_shared_alloc, tls);
	if (stack)
		uk_free(sched->allocator, stack);
#if CONFIG_LIBFLEXOS_INTELPKU
	/* TODO FLEXOS free() per-compartment stacks */
	/* Clearly, not doing it now should not be much of an issue because
	 * this error case is unlikely to happen in our benchmarks... */
#endif /* CONFIG_LIBFLEXOS_INTELPKU */
	if (thread)
		uk_free(sched->allocator, thread);

	return NULL;
}
#endif /* CONFIG_LIBFLEXOS_VMEPT */


void uk_sched_thread_destroy(struct uk_sched *sched, struct uk_thread *thread)
{
	UK_ASSERT(sched != NULL);
	UK_ASSERT(thread != NULL);
	UK_ASSERT(thread->stack != NULL);
	UK_ASSERT(!have_tls_area() || thread->tls != NULL);
	UK_ASSERT(is_exited(thread));

	UK_TAILQ_REMOVE(&sched->exited_threads, thread, thread_list);
	uk_thread_fini(thread, sched->allocator);
	uk_free(sched->allocator, thread->stack);
#if CONFIG_LIBFLEXOS_INTELPKU
	/* TODO FLEXOS free() per-compartment stacks */
#endif /* CONFIG_LIBFLEXOS_INTELPKU */
#if CONFIG_LIBFLEXOS_VMEPT
	/* here we need to detroy the associated rpc thread in each other compartment */
	// TODO: error handling
	for (size_t i = 0; i < FLEXOS_VMEPT_COMP_COUNT; ++i) {
		if (i == flexos_vmept_comp_id)
			continue;
		flexos_vmept_master_rpc_call_destroy(flexos_vmept_comp_id, i, thread->tid);
	}
#endif /* CONFIG_LIBFLEXOS_VMEPT */
	if (thread->tls)
		uk_free(flexos_shared_alloc, thread->tls);
	uk_free(sched->allocator, thread);
}


#if CONFIG_LIBFLEXOS_VMEPT
void uk_sched_thread_destroy_rpc_only(struct uk_sched *sched, struct uk_thread *thread,
	uint8_t normal_thread_comp_id, uint8_t normal_thread_tid,
	volatile struct flexos_vmept_thread_map *thread_map)
{
	UK_ASSERT(sched != NULL);
	UK_ASSERT(thread != NULL);
	UK_ASSERT(thread->stack != NULL);
	UK_ASSERT(!have_tls_area() || thread->tls != NULL);
	UK_ASSERT(is_exited(thread));

	UK_TAILQ_REMOVE(&sched->exited_threads, thread, thread_list);
	uk_thread_fini(thread, sched->allocator);
	uk_free(sched->allocator, thread->stack);

	if (thread->tls)
		uk_free(flexos_shared_alloc, thread->tls);
	uk_free(sched->allocator, thread);
	if (thread_map) {
		flexos_vmept_thread_map_put(thread_map, normal_thread_comp_id,
			(uint8_t) normal_thread_tid, NULL);
	}
}
#endif /* CONFIG_LIBFLEXOS_VMEPT */

void uk_sched_thread_kill(struct uk_sched *sched, struct uk_thread *thread)
{
	uk_sched_thread_remove(sched, thread);
}

void uk_sched_thread_sleep(__nsec nsec)
{
	struct uk_thread *thread;

	thread = uk_thread_current();
	uk_thread_block_timeout(thread, nsec);
	uk_sched_yield();
}

void uk_sched_thread_exit(void)
{
	struct uk_thread *thread;

	thread = uk_thread_current();
	UK_ASSERT(thread->sched);
	uk_sched_thread_remove(thread->sched, thread);
	UK_CRASH("Failed to stop the thread\n");
}
