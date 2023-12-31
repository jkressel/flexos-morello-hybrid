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

#include <uk/mutex.h>
#include <uk/arch/time.h>
#include <lwip/sys.h>

#include <uk/essentials.h>
#include <flexos/isolation.h>

/**
 * Initializes a new semaphore. The "count" argument specifies
 * the initial state of the semaphore.
 */
err_t sys_mutex_new(sys_mutex_t *mtx)
{
	flexos_gate(libuklock, uk_mutex_init, &mtx->mtx);
	mtx->valid = 1;
	return ERR_OK;
}

int sys_mutex_valid(sys_mutex_t *mtx)
{
	return (mtx->valid == 1);
}

void sys_mutex_set_invalid(sys_mutex_t *mtx)
{
	mtx->valid = 0;
}

void sys_mutex_free(sys_mutex_t *mtx)
{
	sys_mutex_set_invalid(mtx);
}

void sys_mutex_lock(sys_mutex_t *mtx)
{
	flexos_gate(liblock, uk_mutex_lock, &mtx->mtx);
}

/* Signals on mutex. */
void sys_mutex_unlock(sys_mutex_t *mtx)
{
	flexos_gate(liblock, uk_mutex_unlock, &mtx->mtx);
}
