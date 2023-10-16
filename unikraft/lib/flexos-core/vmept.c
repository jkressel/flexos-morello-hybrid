/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2021, Hugo Lefeuvre <hugo.lefeuvre@manchester.ac.uk>
 * 		       Stefan Teodorescu <stefanl.teodorescu@gmail.com>
 * 		       Sebastian Rauch <s.rauch94@gmail.com>
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

#include <flexos/impl/vmept.h>
#include <uk/alloc.h>
#include <uk/assert.h>
#include <uk/sched.h>
#include <uk/init.h>

#include <stdio.h>

/* The RPC shared pages behave like a stack: when a new RPC is made,
 * we push a new page and pop when it returns, similar to how the usual
 * function call stack works.
 *
 * TODO: revisit this model when implementing multithreading. Most probably
 * we'll need one RPC stack per thread.
 */
unsigned long shmem_rpc_page = FLEXOS_VMEPT_RPC_PAGES_ADDR;

volatile uint8_t flexos_vmept_comp_id = FLEXOS_VMEPT_COMP_ID;

// only for testing and debugging
extern int ping1(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern int ping2(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern int ping3(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern int ping4(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern int ping5(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern int ping6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern void reset_runs(void);

extern void pong1(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern void pong2(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern void pong3(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern void pong4(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern void pong5(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);
extern void pong6(int arg1, int arg2, int arg3, int arg4, int arg5, int arg6);

/* just for debugging, to make sure function pointers are the same
 * across compartments  */
void _flexos_vmept_dbg_print_address_info() {
#if FLEXOS_VMEPT_DEBUG_PRINT_ADDR
	static int first = 1;
	if (!first)
		return;

	first = 0;
	printf("printing address info for compartment %d\n", flexos_vmept_comp_id);
	printf("&ping1: %p\n", (void*) &ping1);
	printf("&ping2: %p\n", (void*) &ping2);
	printf("&ping3: %p\n", (void*) &ping3);
	printf("&ping4: %p\n", (void*) &ping4);
	printf("&ping5: %p\n", (void*) &ping5);
	printf("&ping6: %p\n", (void*) &ping6);

	printf("&pong1: %p\n", (void*) &pong1);
	printf("&pong2: %p\n", (void*) &pong2);
	printf("&pong3: %p\n", (void*) &pong3);
	printf("&pong4: %p\n", (void*) &pong4);
	printf("&pong5: %p\n", (void*) &pong5);
	printf("&pong6: %p\n", (void*) &pong6);

	printf("&reset_runs: %p\n", (void*) &reset_runs);
#endif /* FLEXOS_VMEPT_DEBUG_PRINT_ADDR */
}

static inline __attribute__((always_inline)) flexos_vmept_master_rpc_lock(struct flexos_vmept_master_rpc_ctrl *master_ctrl, int lock_value)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(master_ctrl: %p) Lock value: %08x, desired: %08x\n", master_ctrl, master_ctrl->lock, lock_value));
	int expected = 0;
	while (!__atomic_compare_exchange_n(&master_ctrl->lock, &expected, lock_value, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
		expected = 0;
		uk_sched_yield();
	}
	FLEXOS_VMEPT_DEBUG_PRINT(("Acquired lock for master rpc ctrl %p.\n", master_ctrl));
}


static inline __attribute__((always_inline)) flexos_vmept_master_rpc_unlock(struct flexos_vmept_master_rpc_ctrl *master_ctrl)
{
	master_ctrl->lock = 0;
	FLEXOS_VMEPT_DEBUG_PRINT(("Released lock for master rpc ctrl %p.\n", master_ctrl));
}

/* The retun value of this funtion indicates whether there was a return or not:
 * 0 means no return value, 1 means there was a return vale.
 * If there is a return value, it is written to out_ret. */
static int flexos_vmept_eval_func(struct flexos_vmept_rpc_ctrl *ctrl, uint64_t *out_ret)
{
	uint64_t finfo = ctrl->f_info;
	uint8_t argc = FLEXOS_VMEPT_FINFO_EXTRACT_ARGC(finfo);
	UK_ASSERT(argc <= FLEXOS_VMEPT_MAX_PARAMS);

	/*
	uint64_t args[FLEXOS_VMEPT_MAX_PARAMS];
	for (size_t i = 0; i < FLEXOS_VMEPT_MAX_PARAMS; ++i) {
		args[i] = ctrl->parameters[i];
	} */

	uint8_t key_to = flexos_vmept_extract_key_to(ctrl->extended_state);
	FLEXOS_VMEPT_DEBUG_PRINT(("Executing function at %p in compartment %ld, finfo=%016lx.\n",
		ctrl->f_ptr, (int) key_to, finfo));

	// rax is unused untill the call, so we use it to store the pointer
	register uint64_t ret asm("rax") = (uint64_t) ctrl->f_ptr;

	asm volatile (
	"cmp $0, %[argc]		\n"
	"jz 1f				\n"
	"movq 0(%[args]), %%rdi		\n"
	"cmp $1, %[argc]		\n"
	"jz 1f				\n"
	"movq 8(%[args]), %%rsi		\n"
	"cmp $2, %[argc]		\n"
	"jz 1f				\n"
	"movq 16(%[args]), %%rdx	\n"
	"cmp $3, %[argc]		\n"
	"jz 1f				\n"
	"movq 24(%[args]), %%rcx	\n"
	"cmp $3, %[argc]		\n"
	"jz 1f				\n"
	"movq 32(%[args]), %%r8		\n"
	"cmp $5, %[argc]		\n"
	"jz 1f				\n"
	"movq 40(%[args]), %%r9		\n"
	"1:				\n"
	"call *%[ret]			\n"
	"movq %%rax, %[ret]		\n"
	: /* output constraints */
	  [ret] "+&r" (ret)
	: /* input constraints */
	  [args] "r" (ctrl->parameters),
	  [argc] "r" (argc)
	: /* clobbers */
	  "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory"
	);

	// copy so that a function call for debugging can safely overwrite rax (which holds ret)
	uint64_t ret_copy = ret;
	if (FLEXOS_VMEPT_FINFO_EXTRACT_RET(finfo)) {
		FLEXOS_VMEPT_DEBUG_PRINT(("return value after call: %016lx\n", ret_copy));
		*out_ret = ret_copy;
		return 1;
	}
	return 0;
}


/* wait for the RPC call to finish */
void flexos_vmept_wait_for_rpc(volatile struct flexos_vmept_rpc_ctrl *ctrl)
{
	uint64_t ext_state;
	int state_const;
	uint8_t key_from;
	uint8_t key_to;
	int has_ret;
	uint64_t retval;
	FLEXOS_VMEPT_DEBUG_PRINT(("Comp %d waiting for call to finish.\n", flexos_vmept_comp_id));
	while (1) {
		ext_state = ctrl->extended_state;
		state_const = flexos_vmept_extract_state(ext_state) & FLEXOS_VMEPT_RPC_STATE_CONSTANT_MASK;
		key_from = flexos_vmept_extract_key_from(ext_state);
		key_to = flexos_vmept_extract_key_to(ext_state);
		if (state_const == FLEXOS_VMEPT_RPC_STATE_CALLED && key_to == flexos_vmept_comp_id) {
			// handle nested rpc call
			FLEXOS_VMEPT_DEBUG_PRINT(("Handling nested call.\n"));
			has_ret = flexos_vmept_eval_func(ctrl, &retval);
			flexos_vmept_ctrl_set_state(ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN);
			if (has_ret)
				flexos_vmept_ctrl_set_ret(ctrl, retval);
			flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_RETURNED, flexos_vmept_comp_id, key_from);
		} else if (state_const == FLEXOS_VMEPT_RPC_STATE_RETURNED && key_to == flexos_vmept_comp_id) {
			// return from rpc call
			FLEXOS_VMEPT_DEBUG_PRINT(("Comp %d finished call.\n", flexos_vmept_comp_id));
			return;
		} else {
			uk_sched_yield();
		}
	}
}

void flexos_vmept_rpc_loop()
{
	_flexos_vmept_dbg_print_address_info();	// to make sure addresses match

	volatile struct flexos_vmept_rpc_ctrl *ctrl = NULL;
	/* make sure the ctrl field in the thread was set */ // TODO: is this nesessary?
	while ((ctrl = (volatile struct flexos_vmept_rpc_ctrl *) uk_thread_current()->ctrl) == NULL) {
		uk_sched_yield();
	}

	FLEXOS_VMEPT_DEBUG_PRINT(("Starting RPC server, observing ctrl %p\n", ctrl));

	uint64_t ext_state;
	int state_const;
	uint8_t  key_from;
	uint8_t key_to;
	int has_ret;
	uint64_t retval;
	while(1) {
		ext_state = ctrl->extended_state;
		state_const = flexos_vmept_extract_state(ext_state) & FLEXOS_VMEPT_RPC_STATE_CONSTANT_MASK;
		key_from = flexos_vmept_extract_key_from(ext_state);
		key_to = flexos_vmept_extract_key_to(ext_state);
		if (state_const == FLEXOS_VMEPT_RPC_STATE_CALLED && key_to == flexos_vmept_comp_id) {
			// handle rpc call to this compartment
			FLEXOS_VMEPT_DEBUG_PRINT(("Comp %d handling call from %d.\n", key_to, key_from));
			has_ret = flexos_vmept_eval_func(ctrl, &retval);
			flexos_vmept_ctrl_set_state(ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN);
			if (has_ret)
				flexos_vmept_ctrl_set_ret(ctrl, retval);
			flexos_vmept_ctrl_set_extended_state(ctrl, FLEXOS_VMEPT_RPC_STATE_RETURNED, flexos_vmept_comp_id, key_from);
			FLEXOS_VMEPT_DEBUG_PRINT(("Comp %d returning from call made from %d.\n", key_to, key_from));
		} else if (state_const == FLEXOS_VMEPT_RPC_STATE_RETURNED && key_to == flexos_vmept_comp_id) {
			// returns should never arrive here
			printf("Unexpected return in rpc loop. This is a bug!\n");
		} else {
			uk_sched_yield();
		}
	}
}

void flexos_vmept_master_rpc_loop()
{
	static struct flexos_vmept_thread_map thread_map;
	flexos_vmept_thread_map_init(&thread_map);
	volatile struct flexos_vmept_master_rpc_ctrl *ctrl = flexos_vmept_master_rpc_ctrl(flexos_vmept_comp_id);
	flexos_vmept_init_master_rpc_ctrl(ctrl);

	FLEXOS_VMEPT_DEBUG_PRINT(("Starting master rpc loop. Observing master_rpc_ctrl at %p\n", ctrl));
	while (1) {
		if (ctrl->state == FLEXOS_VMEPT_MASTER_RPC_STATE_CALLED && ctrl->to == flexos_vmept_comp_id) {
			FLEXOS_VMEPT_DEBUG_PRINT(("Received master rpc call at %p.\n", ctrl));

			int tid = ctrl->local_tid;
			UK_ASSERT(tid >= 0 && tid < FLEXOS_VMEPT_MAX_THREADS);
			uint8_t calling_comp = ctrl->from;
			struct uk_thread *thread = NULL;
			struct uk_sched *sched = uk_thread_current()->sched;
			// there are only two actions: create or destroy a thread
			switch (ctrl->action) {
				case FLEXOS_VMEPT_MASTER_RPC_ACTION_CREATE:
					// TODO: error handling
					FLEXOS_VMEPT_DEBUG_PRINT(("Handling create.\n"));
					thread = uk_sched_thread_create_rpc_only(sched, NULL, NULL, flexos_vmept_rpc_loop, NULL, ctrl->from, tid, &thread_map);
					ctrl->state = FLEXOS_VMEPT_MASTER_RPC_STATE_RETURNED;
					FLEXOS_VMEPT_DEBUG_PRINT(("Created thread with tid %d (ptr: %p) to handle RPC calls from thread with tid %d in compartment %d.\n", thread->tid, thread, tid, calling_comp));
					FLEXOS_VMEPT_DEBUG_PRINT(("Mapping is set up to track (comp=%d, local_tid=%d) -> %p.\n", calling_comp, tid, flexos_vmept_thread_map_lookup(&thread_map, calling_comp, tid)));
					break;
				case FLEXOS_VMEPT_MASTER_RPC_ACTION_DESTROY:
					FLEXOS_VMEPT_DEBUG_PRINT(("Handling destroy.\n"));
					thread = flexos_vmept_thread_map_lookup(&thread_map, calling_comp, (uint8_t) tid);
					UK_ASSERT(thread);
					// TODO: error handling
					FLEXOS_VMEPT_DEBUG_PRINT(("Destroying thread with tid %d (ptr: %p) handling RPC calls from thread with tid %d in compartment %d.\n", thread->tid, thread, tid, calling_comp));
					uk_sched_thread_destroy_rpc_only(sched, thread, calling_comp, (uint8_t) tid, &thread_map);
					FLEXOS_VMEPT_DEBUG_PRINT(("Mapping is set up to track (comp=%d, local_tid=%d) -> %p.\n", calling_comp, tid, flexos_vmept_thread_map_lookup(&thread_map, calling_comp, tid)));
					ctrl->state = FLEXOS_VMEPT_MASTER_RPC_STATE_RETURNED;
					break;
				default:
					printf("Bad action. This is a bug!\n");
			}
			// TODO: error handling ?
			ctrl->state = FLEXOS_VMEPT_BUILD_MASTER_RPC_RETURN_STATE(0);
		} else {
			uk_sched_yield();
		}
	}
}

int flexos_vmept_master_rpc_call(uint8_t key_from, uint8_t key_to, uint8_t local_tid, uint8_t action)
{
	volatile struct flexos_vmept_master_rpc_ctrl *master_ctrl = flexos_vmept_master_rpc_ctrl(key_to);
	FLEXOS_VMEPT_DEBUG_PRINT(("Making master rpc call from comp %d to comp %d (master_ctrl at %p) with local_tid=%d, action=%d.\n", key_from, key_to, master_ctrl, local_tid, action));
	FLEXOS_VMEPT_DEBUG_PRINT(("Before init lock.\n"));
	while (! master_ctrl->initialized) {
		uk_sched_yield();
	}
	FLEXOS_VMEPT_DEBUG_PRINT(("Past init lock.\n"));

	int lock_value = flexos_vmept_build_lock_value(local_tid);
	flexos_vmept_master_rpc_lock(master_ctrl, lock_value);

	master_ctrl->from = key_from;
	master_ctrl->to = key_to;
	master_ctrl->local_tid = local_tid;
	master_ctrl->action = action;

	// important: state should always be the last field that is set
	master_ctrl->state = FLEXOS_VMEPT_MASTER_RPC_STATE_CALLED;

	// wait for call to return
	while ((master_ctrl->state & FLEXOS_VMEPT_MASTER_RPC_STATE_CONSTANT_MASK) != FLEXOS_VMEPT_MASTER_RPC_STATE_RETURNED) {
		uk_sched_yield();
	}

	// TODO: error handling
	int ret = FLEXOS_VMEPT_MASTER_RPC_STATE_EXTRACT_VALUE(master_ctrl->state);
	master_ctrl->state = FLEXOS_VMEPT_MASTER_RPC_STATE_IDLE;
	flexos_vmept_master_rpc_unlock(master_ctrl);
	return ret;
}


void flexos_vmept_create_rpc_loop_thread()
{
	struct uk_thread *thread = uk_thread_current();
	uk_sched_thread_create_rpc_only(thread->sched, NULL, NULL, &flexos_vmept_master_rpc_loop, NULL,
		flexos_vmept_comp_id, 0, NULL);
}

uk_lib_initcall(flexos_vmept_create_rpc_loop_thread);
