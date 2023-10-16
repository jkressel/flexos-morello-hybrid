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

#ifndef FLEXOS_H
#define FLEXOS_H

#include <uk/config.h>

/* Enable/Disable Intel MPK/PKU support */
#if CONFIG_LIBFLEXOS_INTELPKU
#include <flexos/impl/intelpku.h>
#include <flexos/impl/intelpku-impl.h>
#else
/* If we build with these gates without CONFIG_LIBFLEXOS_INTELPKU
 * then there is a configuration mistake */
#define flexos_intelpku_gate(...) UK_CTASSERT(0)
#define flexos_intelpku_gate_r(...) UK_CTASSERT(0)
#endif /* CONFIG_LIBFLEXOS_INTELPKU */

#if CONFIG_LIBFLEXOS_MORELLO
#include <flexos/impl/morello.h>
#include <flexos/impl/morello-impl.h>
#endif

/* Enable/Disable VM/EPT support */
#if CONFIG_LIBFLEXOS_VMEPT
#include <flexos/impl/vmept.h>
#else
/* If we build with these gates without CONFIG_LIBFLEXOS_VMEPT
 * then there is a configuration mistake */
#define flexos_vmept_gate(...) UK_CTASSERT(0)
#define flexos_vmept_gate_r(...) UK_CTASSERT(0)
#endif /* CONFIG_LIBFLEXOS_VMEPT */

/* Build with function call instanciation (debugging) */
#if (!CONFIG_LIBFLEXOS_INTELPKU && !CONFIG_LIBFLEXOS_VMEPT)
#include <uk/alloc.h>
#define flexos_shared_alloc _uk_alloc_head
//#define flexos_comp0_alloc _uk_alloc_head
#endif /* (!CONFIG_LIBFLEXOS_INTELPKU && !CONFIG_LIBFLEXOS_VMEPT) */

/* Do not build with gate placeholders. These should be replaced by the
 * toolchain before build. Encountering them at build time is almost
 * certainly a bug.
 */
#define flexos_gate_r(...) UK_CTASSERT(0)
#define flexos_gate(...) UK_CTASSERT(0)
#define flexos_malloc_whitelist(...) UK_CTASSERT(0)
#define flexos_calloc_whitelist(...) UK_CTASSERT(0)
#define flexos_palloc_whitelist(...) UK_CTASSERT(0)
#define flexos_free_whitelist(...) UK_CTASSERT(0)

/* NOP gate, this is just a function call. This gate is inserted whenever
 * a cross-microlibrary call is realized within a compartment.
 */
#define flexos_nop_gate(key_from, key_to, func, ...) func(__VA_ARGS__)
#define flexos_nop_gate_r(key_from, key_to, ret, func, ...) ret = func(__VA_ARGS__)

#include <flexos/literals.h>

#endif /* FLEXOS_H */
