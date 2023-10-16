/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Hugo Lefeuvre <hugo.lefeuvre@neclab.eu>
 *
 * Copyright (c) 2020, NEC Europe Ltd., NEC Corporation. All rights reserved.
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

#include <uk/tlsf.h>
#include <uk/alloc_impl.h>
#include <tlsf.h>
#include <uk/assert.h>

/* malloc interface */

static void *uk_tlsf_malloc(struct uk_alloc *a, size_t size)
{
	return tlsf_malloc(size, (void *)((uintptr_t) a +
					sizeof(struct uk_alloc)));
}

static void uk_tlsf_free(struct uk_alloc *a, void *ptr)
{
#if CONFIG_LIBFLEXOS_DEBUG
	/* With FlexOS we have a lot of allocators operating on the memory.
	 * Doing these checks allows us to catch invalid free()s early on */
	if ((uintptr_t)ptr > ((uintptr_t)a) + a->len ||
	    (uintptr_t)ptr < (uintptr_t)a) {
		UK_CRASH("Called with a pointer that probably doesn't belong "
			 "to this allocator; uk_tlsf_free(%p, %p) -> not in [%p, %p]\n",
			 a, ptr, a, (void*)(((uintptr_t)a) + a->len));
	}
#endif
	tlsf_free(ptr, (void *)((uintptr_t) a + sizeof(struct uk_alloc)));
}

/* initialization */

struct uk_alloc *uk_tlsf_init(void *base, size_t len)
{
	struct uk_alloc *a;
	size_t res;

	/* enough space for allocator available? */
	if (sizeof(*a) > len) {
		uk_pr_err("Not enough space for allocator: %" __PRIsz
			  " B required but only %" __PRIuptr" B usable\n",
			  sizeof(*a), len);
		return NULL;
	}

	/* store allocator metadata on the heap, just before the memory pool */
	a = (struct uk_alloc *)base;
	a->len = len;
	uk_pr_info("Initialize tlsf allocator @ 0x%" __PRIuptr ", len %"
			__PRIsz"\n", (uintptr_t)a, len);

	res = init_memory_pool(len - sizeof(*a), base + sizeof(*a));
	if (res == (size_t)-1)
		return NULL;

	uk_alloc_init_malloc_ifmalloc(a, uk_tlsf_malloc, uk_tlsf_free, NULL);

	return a;
}
