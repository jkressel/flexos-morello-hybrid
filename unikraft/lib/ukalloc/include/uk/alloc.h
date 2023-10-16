/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *          Florian Schmidt <florian.schmidt@neclab.eu>
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

#ifndef __UK_ALLOC_H__
#define __UK_ALLOC_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <uk/config.h>
#include <uk/assert.h>
#include <uk/essentials.h>

struct uk_alloc;

#ifdef __cplusplus
extern "C" {
#endif

#define uk_zalloc(a, size)  uk_calloc(a, 1, size)
#define uk_do_zalloc(a, size) uk_do_calloc(a, 1, size)

typedef void* (*uk_alloc_malloc_func_t)
		(struct uk_alloc *a, size_t size);
typedef void* (*uk_alloc_calloc_func_t)
		(struct uk_alloc *a, size_t nmemb, size_t size);
typedef int   (*uk_alloc_posix_memalign_func_t)
		(struct uk_alloc *a, void **memptr, size_t align, size_t size);
typedef void* (*uk_alloc_memalign_func_t)
		(struct uk_alloc *a, size_t align, size_t size);
typedef void* (*uk_alloc_realloc_func_t)
		(struct uk_alloc *a, void *ptr, size_t size);
typedef void  (*uk_alloc_free_func_t)
		(struct uk_alloc *a, void *ptr);
typedef void* (*uk_alloc_palloc_func_t)
		(struct uk_alloc *a, unsigned long num_pages);
typedef void  (*uk_alloc_pfree_func_t)
		(struct uk_alloc *a, void *ptr, unsigned long num_pages);
typedef int   (*uk_alloc_addmem_func_t)
		(struct uk_alloc *a, void *base, size_t size);
#if CONFIG_LIBUKALLOC_IFSTATS
typedef ssize_t (*uk_alloc_availmem_func_t)
		(struct uk_alloc *a);
#endif

struct uk_alloc {
	/* memory allocation */
	uk_alloc_malloc_func_t malloc;
	uk_alloc_calloc_func_t calloc;
	uk_alloc_realloc_func_t realloc;
	uk_alloc_posix_memalign_func_t posix_memalign;
	uk_alloc_memalign_func_t memalign;
	uk_alloc_free_func_t free;

#if CONFIG_LIBUKALLOC_IFMALLOC
	uk_alloc_free_func_t free_backend;
	uk_alloc_malloc_func_t malloc_backend;
#endif

	/* page allocation interface */
	uk_alloc_palloc_func_t palloc;
	uk_alloc_pfree_func_t pfree;
#if CONFIG_LIBUKALLOC_IFSTATS
	/* optional interface */
	uk_alloc_availmem_func_t availmem;
#endif
	/* optional interface */
	uk_alloc_addmem_func_t addmem;
	size_t len;

	/* internal */
	struct uk_alloc *next;
	int8_t priv[];
};

extern struct uk_alloc *_uk_alloc_head;

#if CONFIG_LIBFLEXOS_INTELPKU
#include <flexos/impl/intelpku.h>

/* FIXME FLEXOS: It seems that GCC optimizations modify this code so that
 * flexos_comp1_alloc is read even if it is not the current compartment's
 * allocator. Obviously this leads to a PKU protection fault. For now
 * we simply disable optimizations for this function, but this is a dirty
 * workaround.
 */

#pragma GCC push_options
#pragma GCC optimize("O0")
#endif

#if CONFIG_LIBFLEXOS_MORELLO
#include <flexos/impl/morello.h>
#endif
static inline struct uk_alloc *uk_alloc_get_default(void)
{
#if CONFIG_LIBFLEXOS_INTELPKU
	uint32_t pkru = rdpkru();

	/* Use the allocator the corresponds to the current
	 * compartment. */
	/* FLEXOS TODO this code should be generated */
	switch (pkru) {
	case 0x3ffffffc:
		return _uk_alloc_head;
	case ( 0x3fffffff & ( ~ (0x3 << (1 + 1) ) ) ):
		return flexos_comp1_alloc;

	case 0x3fffffff:
		/* reserved for shared data */
                __attribute__((fallthrough));
	default:
		uk_pr_debug("Allocating from a context where the current "
			 "compartment cannot be clearly determined.");
		return _uk_alloc_head;
        }
/* TODO Morello this needs to be generated and inserted */
#elif CONFIG_LIBFLEXOS_MORELLO
	//for testing, do it properly in production though
	int compartment = get_compartment_id();
//int compartment = 1;
	switch (compartment) {
		case 0:
			//return comp0_allocator;
			return _uk_alloc_head;
		case 1: 
			return comp1_allocator;
//			return _uk_alloc_head;
		case 2:
			//return _uk_alloc_head;
			return comp2_allocator;
		default:
			return _uk_alloc_head;
			UK_CRASH("No allocator for compartment %d!", compartment);
	}
#endif /* CONFIG_LIBFLEXOS_INTELPKU */

	return _uk_alloc_head;
}
#if CONFIG_LIBFLEXOS_INTELPKU
#pragma GCC pop_options
#endif

/* wrapper functions */
static inline void *uk_do_malloc(struct uk_alloc *a, size_t size)
{
	UK_ASSERT(a);
	return a->malloc(a, size);
}

static inline void *uk_malloc(struct uk_alloc *a, size_t size)
{
	if (unlikely(!a)) {
		errno = ENOMEM;
		return NULL;
	}
	return uk_do_malloc(a, size);
}

static inline void *uk_do_calloc(struct uk_alloc *a,
				 size_t nmemb, size_t size)
{
	UK_ASSERT(a);
	return a->calloc(a, nmemb, size);
}

static inline void *uk_calloc(struct uk_alloc *a,
			      size_t nmemb, size_t size)
{
	if (unlikely(!a)) {
		errno = ENOMEM;
		return NULL;
	}
	return uk_do_calloc(a, nmemb, size);
}

static inline void *uk_do_realloc(struct uk_alloc *a,
				  void *ptr, size_t size)
{
	UK_ASSERT(a);
	return a->realloc(a, ptr, size);
}

static inline void *uk_realloc(struct uk_alloc *a, void *ptr, size_t size)
{
	if (unlikely(!a)) {
		errno = ENOMEM;
		return NULL;
	}
	return uk_do_realloc(a, ptr, size);
}

static inline int uk_do_posix_memalign(struct uk_alloc *a, void **memptr,
				       size_t align, size_t size)
{
	UK_ASSERT(a);
	return a->posix_memalign(a, memptr, align, size);
}

static inline int uk_posix_memalign(struct uk_alloc *a, void **memptr,
				    size_t align, size_t size)
{
	if (unlikely(!a)) {
		*memptr = NULL;
		return ENOMEM;
	}
	return uk_do_posix_memalign(a, memptr, align, size);
}

static inline void *uk_do_memalign(struct uk_alloc *a,
				   size_t align, size_t size)
{
	UK_ASSERT(a);
	return a->memalign(a, align, size);
}

static inline void *uk_memalign(struct uk_alloc *a,
				size_t align, size_t size)
{
	if (unlikely(!a))
		return NULL;
	return uk_do_memalign(a, align, size);
}

static inline void uk_do_free(struct uk_alloc *a, void *ptr)
{
	UK_ASSERT(a);
	a->free(a, ptr);
}

static inline void uk_free(struct uk_alloc *a, void *ptr)
{
	uk_do_free(a, ptr);
}

static inline void *uk_do_palloc(struct uk_alloc *a, unsigned long num_pages)
{
	UK_ASSERT(a);
	return a->palloc(a, num_pages);
}

static inline void *uk_palloc(struct uk_alloc *a, unsigned long num_pages)
{
	if (unlikely(!a || !a->palloc))
		return NULL;
	return uk_do_palloc(a, num_pages);
}

static inline void uk_do_pfree(struct uk_alloc *a, void *ptr,
			       unsigned long num_pages)
{
	UK_ASSERT(a);
	a->pfree(a, ptr, num_pages);
}

static inline void uk_pfree(struct uk_alloc *a, void *ptr,
			    unsigned long num_pages)
{
	uk_do_pfree(a, ptr, num_pages);
}

static inline int uk_alloc_addmem(struct uk_alloc *a, void *base,
				  size_t size)
{
	UK_ASSERT(a);
	if (a->addmem)
		return a->addmem(a, base, size);
	else
		return -ENOTSUP;
}
#if CONFIG_LIBUKALLOC_IFSTATS
static inline ssize_t uk_alloc_availmem(struct uk_alloc *a)
{
	UK_ASSERT(a);
	if (!a->availmem)
		return (ssize_t) -ENOTSUP;
	return a->availmem(a);
}
#endif /* CONFIG_LIBUKALLOC_IFSTATS */

#ifdef __cplusplus
}
#endif

#endif /* __UK_ALLOC_H__ */
