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

#ifndef FLEXOS_VMEPT_H
#define FLEXOS_VMEPT_H

#include "typecheck.h"

#include <uk/config.h>
#include <uk/essentials.h>
#include <uk/sections.h>
#include <uk/plat/mm.h>
#include <uk/alloc.h>

#include <uk/thread.h>

/* make sure all necessary macros are defined */
#ifndef FLEXOS_VMEPT_COMP_ID 	// this compartment
#error "FLEXOS_VMEPT_COMP_ID must be defined"
#endif
#ifndef FLEXOS_VMEPT_COMP_COUNT // total number of compartments
#error "FLEXOS_VMEPT_COMP_COUNT must be defined"
#endif
#ifndef FLEXOS_VMEPT_APPCOMP 	// compartment containing the app
#error "FLEXOS_VMEPT_APPCOMP must be defined"
#endif

extern volatile uint8_t flexos_vmept_comp_id;


/* to enable/disable debug prints */
#define FLEXOS_VMEPT_DEBUG 0
#define FLEXOS_VMEPT_DEBUG_PRINT_ADDR 0

#if FLEXOS_VMEPT_DEBUG
	#include <stdio.h>
	#define FLEXOS_VMEPT_DEBUG_PRINT(x) printf x
#else
	#define FLEXOS_VMEPT_DEBUG_PRINT(x)
#endif


struct uk_alloc;

/* Shared allocator */
extern struct uk_alloc *flexos_shared_alloc;
/* flexos_comp0_alloc is just an alias for the standard, default allocator:
 * offers memory in domain zero.
 */
#define flexos_comp0_alloc _uk_alloc_head

#define FLEXOS_VMEPT_MAX_THREADS 	256
#define FLEXOS_VMEPT_MAX_COMPS		16

/* we could use FLEXOS_VMEPT_COMP_COUNT instead of FLEXOS_VMEPT_MAX_COMPS, but this produces variable alignment */
#define FLEXOS_VMEPT_THREAD_MAP_SIZE (FLEXOS_VMEPT_MAX_COMPS * FLEXOS_VMEPT_MAX_THREADS)

struct flexos_vmept_thread_map {
	struct uk_thread *threads[FLEXOS_VMEPT_THREAD_MAP_SIZE];
};

static inline void __attribute__((always_inline)) flexos_vmept_thread_map_init(struct flexos_vmept_thread_map *map)
{
	for (size_t i = 0; i < FLEXOS_VMEPT_THREAD_MAP_SIZE; ++i) {
		map->threads[i] = NULL;
	}
}

static inline struct __attribute__((always_inline)) uk_thread *flexos_vmept_thread_map_lookup(
const struct flexos_vmept_thread_map *map, uint8_t comp_id, uint8_t local_tid)
{
	return map->threads[comp_id * FLEXOS_VMEPT_MAX_THREADS + local_tid];
}

static inline void __attribute__((always_inline)) flexos_vmept_thread_map_put(
	struct flexos_vmept_thread_map *map, uint8_t comp_id, uint8_t local_tid, struct uk_thread *thread_ptr)
{
	map->threads[comp_id * FLEXOS_VMEPT_MAX_THREADS + local_tid] = thread_ptr;
}

/* master rpc states are integers and have two parts:
 * the lower 8 bits encode the state constant (see below)
 * the higher 24 bits encode a value that has a state-dependent meaning */
#define FLEXOS_VMEPT_MASTER_RPC_STATE_IDLE 	0
#define FLEXOS_VMEPT_MASTER_RPC_STATE_CALLED 	1
#define FLEXOS_VMEPT_MASTER_RPC_STATE_RETURNED 	2

#define FLEXOS_VMEPT_MASTER_RPC_STATE_CONSTANT_MASK 	0x000000ff
#define FLEXOS_VMEPT_MASTER_RPC_VALUE_PART_MASK 	0xffffff00

#define FLEXOS_VMEPT_BUILD_MASTER_RPC_STATE(state_constant, value) \
(((value) << 8) | ((state_constant) & FLEXOS_VMEPT_MASTER_RPC_STATE_CONSTANT_MASK))

#define FLEXOS_VMEPT_MASTER_RPC_STATE_EXTRACT_VALUE(state) \
((state) >> 8)

/* a return code other than 0 signals an error */
#define FLEXOS_VMEPT_BUILD_MASTER_RPC_RETURN_STATE(ret_code) \
FLEXOS_VMEPT_BUILD_MASTER_RPC_STATE(FLEXOS_VMEPT_MASTER_RPC_STATE_RETURNED, ret_code)

#define FLEXOS_VMEPT_MASTER_RPC_ACTION_CREATE 	1
#define FLEXOS_VMEPT_MASTER_RPC_ACTION_DESTROY 	2

/* (maximum) size for flexos_vmept_master_rpc_ctrl and flexos_vmept_rpc_ctrl */
#define FLEXOS_VMEPT_RPC_CTRL_SIZE 256

// TODO: ensure maximum size of 256 bytes
struct flexos_vmept_master_rpc_ctrl {
	/* to make sure access is atomic always align at 8 byte boundary */
	int lock __attribute__ ((aligned (8)));
	int initialized;
	int state;
	uint8_t action;
	uint8_t from;
	uint8_t to;
	int local_tid;		// tid of the normal thread created
};

static inline void __attribute__((always_inline)) flexos_vmept_init_master_rpc_ctrl(struct flexos_vmept_master_rpc_ctrl *ctrl)
{
	ctrl->lock = 0;
	ctrl->state = FLEXOS_VMEPT_MASTER_RPC_STATE_IDLE;
	ctrl->initialized = 1;
	FLEXOS_VMEPT_DEBUG_PRINT(("Initialized master ctrl at %p.\n", ctrl));
}
/* rpc states are integers and have two parts:
 * the lower 8 bits encode the state constant (see below)
 * the higher 24 bits encode a value that has a state-dependent meaning */

#define FLEXOS_VMEPT_RPC_STATE_IDLE	0
#define FLEXOS_VMEPT_RPC_STATE_FROZEN	1
#define FLEXOS_VMEPT_RPC_STATE_CALLED	2
#define FLEXOS_VMEPT_RPC_STATE_RETURNED	3

#define FLEXOS_VMEPT_RPC_STATE_CONSTANT_MASK 	0x000000ff
#define FLEXOS_VMEPT_RPC_VALUE_PART_MASK 	0xffffff00

#define FLEXOS_VMEPT_BUILD_RPC_STATE(state_constant, value) \
(((value) << 8) | ((state_constant) & FLEXOS_VMEPT_RPC_STATE_CONSTANT_MASK))

#define FLEXOS_VMEPT_RPC_STATE_EXTRACT_VALUE(state) \
((state) >> 8)

#define FLEXOS_VMEPT_FINFO_ARGC_MASK	0x00ff
#define FLEXOS_VMEPT_FINFO_RET_MASK	0xff00

#define FLEXOS_VMEPT_BUILD_FINFO(argc, returns_val) \
((argc & FLEXOS_VMEPT_FINFO_ARGC_MASK) | ((returns_val << 8 ) & FLEXOS_VMEPT_FINFO_RET_MASK))

#define FLEXOS_VMEPT_FINFO_EXTRACT_ARGC(finfo) \
((finfo) & FLEXOS_VMEPT_FINFO_ARGC_MASK)

#define FLEXOS_VMEPT_FINFO_EXTRACT_RET(finfo) \
(((finfo) & FLEXOS_VMEPT_FINFO_RET_MASK) >> 8)

// TODO: esure maximum size of 256 bytes
struct flexos_vmept_rpc_ctrl {
	uint64_t extended_state __attribute__ ((aligned (8)));
	int recursion;
	void *f_ptr;
	uint64_t f_info;
	uint64_t parameters[6];
	uint64_t ret;
};

/* All shared memory areas below are hardcoded for now to these values. For the
 * memory sharing mechanism itself, we use our own shared memory device in QEMU
 * which receives addresses and sizes as parameters for multiple memory areas.
 *
 * TODO: These addresses and sizes should be defined by the toolchain (both
 * here and as parameters when running QEMU).
 */

/* TODO: adapt for up to 256 compartments (?)
 * currently it supports a maximum of 16 */
#define FLEXOS_VMEPT_RPC_PAGES_ADDR	0x800000000
#define FLEXOS_VMEPT_RPC_PAGES_SIZE	((size_t) 16 * 256 * 256) 	// FIXME: use correct size
//#define FLEXOS_VMEPT_RPC_PAGES_SIZE	((size_t) 16 * 256 * 256 + 16 * 256)

/* This memory area is used for the shared heap. */
#define FLEXOS_VMEPT_SHARED_MEM_ADDR	0x4000000000
#define FLEXOS_VMEPT_SHARED_MEM_SIZE	((size_t) 128 * 1024 * 1024)

/* The shared_data section between all Unikraft binaries. The loader places
 * this section directly in the shared memory, so all compartments access the
 * same thing.
 */
#define FLEXOS_VMEPT_SHARED_DATA_ADDR	__SHARED_START
#define FLEXOS_VMEPT_SHARED_DATA_SIZE	((size_t) __SHARED_END - __SHARED_START)

/* the maximum number of parameters of a vmept gate
 * this is because of the calling convention
 * don't simply change this number */
#define FLEXOS_VMEPT_MAX_PARAMS	6

extern unsigned long shmem_rpc_page;

#define FLEXOS_VMEPT_MASTER_RPC_CTRL_BLOCK_START ((uint8_t *) shmem_rpc_page)
#define FLEXOS_VMEPT_MASTER_RPC_CTRL_BLOCK_SIZE ((FLEXOS_VMEPT_RPC_CTRL_SIZE) * (FLEXOS_VMEPT_MAX_COMPS))

#define FLEXOS_VMEPT_RPC_CTRL_BLOCK_START (((uint8_t *) shmem_rpc_page) + FLEXOS_VMEPT_MASTER_RPC_CTRL_BLOCK_SIZE)
#define FLEXOS_VMEPT_RPC_CTRL_BLOCK_SIZE ((FLEXOS_VMEPT_MAX_COMPS) * (FLEXOS_VMEPT_MAX_THREADS) * (FLEXOS_VMEPT_RPC_CTRL_SIZE))

#define flexos_vmept_master_rpc_ctrl(comp_id) \
(volatile struct flexos_vmept_master_rpc_ctrl*) ((FLEXOS_VMEPT_MASTER_RPC_CTRL_BLOCK_START) + (comp_id) * (FLEXOS_VMEPT_RPC_CTRL_SIZE))

/* from the compartment ID of the normal thread an its local tid get the rpc_ctrl struct to listen to */
#define flexos_vmept_rpc_ctrl(comp_id, local_tid) \
(volatile struct flexos_vmept_rpc_ctrl*) (FLEXOS_VMEPT_RPC_CTRL_BLOCK_START + (comp_id) * FLEXOS_VMEPT_MAX_THREADS * FLEXOS_VMEPT_RPC_CTRL_SIZE + FLEXOS_VMEPT_RPC_CTRL_SIZE * local_tid)

/* unique per thread and compartment */
#define flexos_vmept_build_lock_value(local_tid) \
((1 << 16) | (flexos_vmept_comp_id << 8) | ((local_tid) & 0xff))

#define flexos_vmept_master_rpc_call_create(key_from, key_to, local_tid) \
flexos_vmept_master_rpc_call((key_from), (key_to), (local_tid), FLEXOS_VMEPT_MASTER_RPC_ACTION_CREATE)

#define flexos_vmept_master_rpc_call_destroy(key_from, key_to, local_tid) \
flexos_vmept_master_rpc_call((key_from), (key_to), (local_tid), FLEXOS_VMEPT_MASTER_RPC_ACTION_DESTROY)

static inline __attribute__((always_inline)) void flexos_vmept_init_master_rpc_ctrls()
{
	for (size_t i = 0; i < FLEXOS_VMEPT_MASTER_RPC_CTRL_BLOCK_SIZE; ++i) {
		((uint8_t *) FLEXOS_VMEPT_MASTER_RPC_CTRL_BLOCK_START)[i] = 0;
	}
	FLEXOS_VMEPT_DEBUG_PRINT(("Zero-initialized master rpc control data structures.\n"));
}

int flexos_vmept_master_rpc_call(uint8_t key_from, uint8_t key_to, uint8_t local_tid, uint8_t action);

int flexos_vmept_master_rpc_call_main(uint8_t key_from, uint8_t key_to, uint8_t local_tid, uint8_t action);

void flexos_vmept_wait_for_rpc();

void flexos_vmept_master_rpc_loop();
void flexos_vmept_rpc_loop();

struct uk_thread;


static inline __attribute__((always_inline)) int flexos_vmept_extract_state(uint64_t extended_state)
{
	return (int) extended_state;
}

static inline __attribute__((always_inline)) uint8_t flexos_vmept_extract_key_from(uint64_t extended_state)
{
	return (uint8_t) ((extended_state >> 32) & 0xff);
}

static inline __attribute__((always_inline)) uint8_t flexos_vmept_extract_key_to(uint64_t extended_state)
{
	return (uint8_t) ((extended_state >> 40) & 0xff);
}

/* to facilitate debugging */
static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_extended_state(
	volatile struct flexos_vmept_rpc_ctrl *ctrl, int state, uint8_t key_from, uint8_t key_to)
{
	uint64_t ext_state = (((uint64_t) key_from) << 32) | (((uint64_t) key_to) << 40) | ((uint32_t) state);
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting extended_state to %016lx.\n", ctrl, flexos_vmept_comp_id, ext_state));
	ctrl->extended_state = ext_state;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_state(
	volatile struct flexos_vmept_rpc_ctrl *ctrl, int state)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting state to %d.\n", ctrl, flexos_vmept_comp_id, state));
	uint8_t from = flexos_vmept_extract_key_from(ctrl->extended_state);
	uint8_t to = flexos_vmept_extract_key_to(ctrl->extended_state);
	flexos_vmept_ctrl_set_extended_state(ctrl, state, from, to);
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_inc_recursion(
	volatile struct flexos_vmept_rpc_ctrl *ctrl)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d inc recursion from %d to %d.\n", ctrl, flexos_vmept_comp_id,
		ctrl->recursion, ctrl->recursion + 1));
	ctrl->recursion++;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_dec_recursion(
	volatile struct flexos_vmept_rpc_ctrl *ctrl)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d dec recursion from %d to %d.\n", ctrl, flexos_vmept_comp_id,
		ctrl->recursion, ctrl->recursion - 1));
	ctrl->recursion--;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_func(
	volatile struct flexos_vmept_rpc_ctrl *ctrl, void *fptr, uint8_t argc, uint8_t returns_val)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting f_ptr to %p and f_info to %016lx.\n", ctrl, flexos_vmept_comp_id,
		fptr, FLEXOS_VMEPT_BUILD_FINFO(argc, returns_val)));
	ctrl->f_ptr = fptr;
	ctrl->f_info = FLEXOS_VMEPT_BUILD_FINFO(argc, returns_val);
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_ret(
	volatile struct flexos_vmept_rpc_ctrl *ctrl, uint64_t ret)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting ret to %016lx.\n", ctrl, flexos_vmept_comp_id, ret));
	ctrl->ret = ret;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_args1(
	volatile struct flexos_vmept_rpc_ctrl *ctrl, uint64_t arg1)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting %d args (%016lx).\n",
		ctrl, flexos_vmept_comp_id, 1, arg1));
	ctrl->parameters[0] = arg1;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_args2(
	volatile struct flexos_vmept_rpc_ctrl *ctrl, uint64_t arg1, uint64_t arg2)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting %d args (%016lx, %016lx).\n",
		ctrl, flexos_vmept_comp_id, 2, arg1, arg2));
	ctrl->parameters[0] = arg1;
	ctrl->parameters[1] = arg2;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_args3(
	volatile struct flexos_vmept_rpc_ctrl *ctrl,
	uint64_t arg1, uint64_t arg2, uint64_t arg3)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting %d args (%016lx, %016lx, %016lx).\n",
		ctrl, flexos_vmept_comp_id, 3, arg1, arg2, arg3));
	ctrl->parameters[0] = arg1;
	ctrl->parameters[1] = arg2;
	ctrl->parameters[2] = arg3;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_args4(
	volatile struct flexos_vmept_rpc_ctrl *ctrl,
	uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting %d args (%016lx, %016lx, %016lx, %016lx).\n",
		ctrl, flexos_vmept_comp_id, 4, arg1, arg2, arg3, arg4));
	ctrl->parameters[0] = arg1;
	ctrl->parameters[1] = arg2;
	ctrl->parameters[2] = arg3;
	ctrl->parameters[3] = arg4;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_args5(
	volatile struct flexos_vmept_rpc_ctrl *ctrl,
	uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting %d args (%016lx, %016lx, %016lx, %016lx, %016lx).\n",
		ctrl, flexos_vmept_comp_id, 5, arg1, arg2, arg3, arg4, arg5));
	ctrl->parameters[0] = arg1;
	ctrl->parameters[1] = arg2;
	ctrl->parameters[2] = arg3;
	ctrl->parameters[3] = arg4;
	ctrl->parameters[4] = arg5;
}

static inline __attribute__((always_inline)) void flexos_vmept_ctrl_set_args6(
	volatile struct flexos_vmept_rpc_ctrl *ctrl,
	uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
	FLEXOS_VMEPT_DEBUG_PRINT(("(ctrl %p) comp %d setting %d args (%016lx, %016lx, %016lx, %016lx, %016lx, %016lx).\n",
		ctrl, flexos_vmept_comp_id, 6, arg1, arg2, arg3, arg4, arg5, arg6));
	ctrl->parameters[0] = arg1;
	ctrl->parameters[1] = arg2;
	ctrl->parameters[2] = arg3;
	ctrl->parameters[3] = arg4;
	ctrl->parameters[4] = arg5;
	ctrl->parameters[5] = arg6;
}

static inline __attribute__((always_inline)) void flexos_vmept_init_rpc_ctrl(struct flexos_vmept_rpc_ctrl *ctrl)
{
	ctrl->recursion = 0;
	// key_from and _key_to are set to 0
	ctrl->extended_state = FLEXOS_VMEPT_RPC_STATE_IDLE;
}

#define flexos_vmept_gate0(key_from, key_to, fptr)						\
do {												\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl); 							\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 0, 0);			\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate0_r(key_from, key_to, retval, fptr)					\
do {												\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 0, 1);			\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED); 	\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	(retval) = _gate_internal_ctrl->ret;							\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate1(key_from, key_to, fptr, arg1)					\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 1, 0);			\
	flexos_vmept_ctrl_set_args1(_gate_internal_ctrl, (uint64_t) arg1);			\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate1_r(key_from, key_to, retval, fptr, arg1)				\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 1, 1);			\
	flexos_vmept_ctrl_set_args1(_gate_internal_ctrl, (uint64_t) arg1);			\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	(retval) = _gate_internal_ctrl->ret;							\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate2(key_from, key_to, fptr, arg1, arg2)					\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 2, 0);			\
	flexos_vmept_ctrl_set_args2(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2);	\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate2_r(key_from, key_to, retval, fptr, arg1, arg2)			\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 2, 1);			\
	flexos_vmept_ctrl_set_args2(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2);	\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	(retval) = _gate_internal_ctrl->ret;							\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate3(key_from, key_to, fptr, arg1, arg2, arg3)				\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 3, 0);			\
	flexos_vmept_ctrl_set_args3(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2,	\
	(uint64_t) arg3);									\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate3_r(key_from, key_to, retval, fptr, arg1, arg2, arg3)			\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 3, 1);			\
	flexos_vmept_ctrl_set_args3(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2, 	\
	(uint64_t) arg3);									\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	(retval) = _gate_internal_ctrl->ret;							\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate4(key_from, key_to, fptr, arg1, arg2, arg3, arg4)			\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg4));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 4, 0);			\
	flexos_vmept_ctrl_set_args4(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2, 	\
	(uint64_t) arg3, (uint64_t) arg4);							\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate4_r(key_from, key_to, retval, fptr, arg1, arg2, arg3, arg4)		\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg4));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 4, 1);			\
	flexos_vmept_ctrl_set_args4(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2,	\
	(uint64_t) arg3, (uint64_t) arg4);							\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	(retval) = _gate_internal_ctrl->ret;							\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate5(key_from, key_to, fptr, arg1, arg2, arg3, arg4, arg5)		\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg4));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg5));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 5, 0);			\
	flexos_vmept_ctrl_set_args5(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2,	\
	 (uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5);					\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate5_r(key_from, key_to, retval, fptr, arg1, arg2, arg3, arg4, 		\
	arg5)											\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg4));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg5));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 5, 1);			\
	flexos_vmept_ctrl_set_args5(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2, 	\
	(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5);					\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	(retval) = _gate_internal_ctrl->ret;							\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate6(key_from, key_to, fptr, arg1, arg2, arg3, arg4, arg5, arg6)		\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg4));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg5));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg6));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 6, 0);			\
	flexos_vmept_ctrl_set_args6(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2, 	\
	(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5, (uint64_t) arg6);			\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

#define flexos_vmept_gate6_r(key_from, key_to, retval, fptr, arg1, arg2, arg3, arg4, 		\
	arg5, arg6)										\
do {												\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg1));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg2));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg3));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg4));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg5));				\
	UK_CTASSERT(flexos_is_heuristic_typeclass_integer(arg6));				\
	volatile struct flexos_vmept_rpc_ctrl *_gate_internal_ctrl = uk_thread_current()->ctrl;	\
	UK_ASSERT(_gate_internal_ctrl);								\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	flexos_vmept_ctrl_set_func(_gate_internal_ctrl, (void *) fptr, 6, 1);			\
	flexos_vmept_ctrl_set_args6(_gate_internal_ctrl, (uint64_t) arg1, (uint64_t) arg2, 	\
	(uint64_t) arg3, (uint64_t) arg4, (uint64_t) arg5, (uint64_t) arg6);			\
	flexos_vmept_ctrl_inc_recursion(_gate_internal_ctrl);					\
	flexos_vmept_ctrl_set_extended_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_CALLED, key_from, key_to); 	\
	flexos_vmept_wait_for_rpc(_gate_internal_ctrl);						\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_FROZEN); 	\
	(retval) = _gate_internal_ctrl->ret;							\
	flexos_vmept_ctrl_dec_recursion(_gate_internal_ctrl);		 			\
	flexos_vmept_ctrl_set_state(_gate_internal_ctrl, FLEXOS_VMEPT_RPC_STATE_IDLE); 		\
} while (0)

/* A variation of the argument counting trick to choose the right gate based on the
 * number of arguments given. Don't use more than 6 arguments!  */
#define CHOOSE_GATE(dummy, g6, g5, g4, g3, g2, g1, g0, ...) g0

/* flexos_vmept_gate(1, 0, printf, "hello\n")
 * -> execute printf("hello\n") is protection domain (VM) 0 */
#define flexos_vmept_gate(key_from, key_to, fname, ...)					\
CHOOSE_GATE(dummy, ## __VA_ARGS__,							\
 	flexos_vmept_gate6(key_from, key_to, &(fname), __VA_ARGS__),			\
	flexos_vmept_gate5(key_from, key_to, &(fname), __VA_ARGS__), 			\
	flexos_vmept_gate4(key_from, key_to, &(fname), __VA_ARGS__), 			\
	flexos_vmept_gate3(key_from, key_to, &(fname), __VA_ARGS__), 			\
	flexos_vmept_gate2(key_from, key_to, &(fname), __VA_ARGS__), 			\
	flexos_vmept_gate1(key_from, key_to, &(fname), __VA_ARGS__), 			\
	flexos_vmept_gate0(key_from, key_to, &(fname)) 					\
)

#define flexos_vmept_gate_r(key_from, key_to, retval, fname, ...)			\
CHOOSE_GATE(dummy, ## __VA_ARGS__,							\
 	flexos_vmept_gate6_r(key_from, key_to, retval, &(fname), __VA_ARGS__),		\
	flexos_vmept_gate5_r(key_from, key_to, retval, &(fname), __VA_ARGS__), 		\
	flexos_vmept_gate4_r(key_from, key_to, retval, &(fname), __VA_ARGS__), 		\
	flexos_vmept_gate3_r(key_from, key_to, retval, &(fname), __VA_ARGS__), 		\
	flexos_vmept_gate2_r(key_from, key_to, retval, &(fname), __VA_ARGS__), 		\
	flexos_vmept_gate1_r(key_from, key_to, retval, &(fname), __VA_ARGS__), 		\
	flexos_vmept_gate0_r(key_from, key_to, retval, &(fname)) 			\
)

#endif /* FLEXOS_VMEPT_H */
