/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2020-2021, Pierre Olivier <pierre.olivier@manchester.ac.uk>
 *                          Hugo Lefeuvre <hugo.lefeuvre@manchester.ac.uk>
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

#include <flexos/impl/intelpku.h>
#include <uk/plat/mm.h>

/* NOTE: no need to check that PKE has been enabled in CR4, this is guaranteed
 * by the platform code as long as CONFIG_HAVE_X86PKU is set. */
UK_CTASSERT(CONFIG_HAVE_X86PKU);

#define PKEY_MASK (~(PAGE_PROT_PKEY0 | PAGE_PROT_PKEY1 | PAGE_PROT_PKEY2 | \
			PAGE_PROT_PKEY3))

#define CLEAR_PKEY(prot)		(prot & PKEY_MASK)
#define INSTALL_PKEY(prot, pkey)	(prot | pkey)

#define __PTE_PKEY_MASK 		(~(_PAGE_PKEY0 | _PAGE_PKEY1 | _PAGE_PKEY2 | _PAGE_PKEY3))
#define GET_PKEY_FROM_PTE(pte)		((pte & ~__PTE_PKEY_MASK) >> 59)

#if CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS
/* Mark it "used" as it might potentially only be used in inline assembly */
struct uk_thread_status_block tsb_comp0[32] __attribute__((used));
/* The toolchain will insert TSB declarations here, e.g.:
 *
 * struct uk_thread_status_block tsb_comp1[32] __section(".data_comp1");
 *
 * for compartment 1.
 */
/* __FLEXOS MARKER__: insert tsb decls here. */
#endif /* CONFIG_LIBFLEXOS_GATE_INTELPKU_PRIVATE_STACKS */

#if CONFIG_LIBFLEXOS_INTELPKU_COUNT_GATE_EXECUTIONS
volatile unsigned long flexos_intelpku_in_gate_counter __section(".data_shared") = 0;
volatile unsigned long flexos_intelpku_out_gate_counter __section(".data_shared") = 0;
#endif /* CONFIG_LIBFLEXOS_COUNT_GATE_EXECUTIONS */

/* Set RO permission for key 'key' in variable val intented to be used as a
 * PKRU value */
static int pkru_set_ro(uint8_t key, uint32_t *val)
{
	if(key > 15)
		return -EINVAL;

	*val &= ~(1UL << (key * 2));
	*val |= 1UL << ((key*2) + 1);

	return 0;
}

/* Set all (RW) permissions for key 'key' in variable val intented to be used
 * as a PKRU value */
static int pkru_set_rw(uint8_t key, uint32_t *val)
{
	if(key > 15)
		return -EINVAL;

	*val &= ~(1UL << (key*2));
	*val &= ~(1UL << ((key*2) + 1));
        return 0;
}

/* Set no access permission for key 'key' in variable val intented to be used
 * as a PKRU value. Remember that PKU does not fault on instruction fetch */
static int pkru_set_no_access(uint8_t key, uint32_t *val)
{
	if(key > 15)
		return -EINVAL;

	*val |= 1UL << (key * 2);
        *val |= 1UL << ((key * 2) + 1);
	return 0;
}

/* Set the key associated with passed set of pages to key */
int flexos_intelpku_mem_set_key(void *paddr, uint64_t npages, uint8_t key)
{
	unsigned long prot, pte, pkey = 0;
	int err = 0;

	/* convert key to be stored in pkey (usually the same as
	 * pkey = key << 4) */
	if (key & 0x01)
		pkey |= PAGE_PROT_PKEY0;
	if (key & 0x02)
		pkey |= PAGE_PROT_PKEY1;
	if (key & 0x04)
		pkey |= PAGE_PROT_PKEY2;
	if (key & 0x08)
		pkey |= PAGE_PROT_PKEY3;

	/* treat each page separately; we don't want to loose W/X permissions */
	for (uint64_t i = 0; i < npages; i++) {
		pte = uk_virt_to_pte((unsigned long) paddr + i * PAGE_SIZE);
		if (pte == PAGE_NOT_MAPPED) {
			uk_pr_info("error: page not mapped (%p)\n", paddr);
			err = -1;
			break;
		}

		/* retrieve current page protections */
		prot = pte |= (PAGE_PROT_WRITE & PAGE_PROT_EXEC);
		/* clear current pkey */
		prot = CLEAR_PKEY(prot);
		/* install new pkey */
		prot = INSTALL_PKEY(prot, pkey);
		/* set new page protections */
		err = uk_page_set_prot((unsigned long) paddr + i * PAGE_SIZE, prot);

		if (err) {
			uk_pr_info("error: unable to set protections\n");
			break;
		}
	}

	return err;
}

/* Get the key associated with passed page */
int flexos_intelpku_mem_get_key(void *paddr)
{
	unsigned long pte;

	pte = uk_virt_to_pte((unsigned long) paddr);
	if (pte == PAGE_NOT_MAPPED) {
		return -1;
	}

	return GET_PKEY_FROM_PTE(pte);
}

/* Set the permission of the calling thread to 'perm' for the key 'key', update
 * the PKRU in the process. */
int flexos_intelpku_set_perm(uint8_t key, flexos_intelpku_perm perm)
{
	uint32_t pkru;

	pkru = rdpkru();

	switch(perm) {
	case PKU_RW:
		pkru_set_rw(key, &pkru);
		break;

	case PKU_RO:
		pkru_set_ro(key, &pkru);
		break;

	case PKU_NONE:
		pkru_set_no_access(key, &pkru);
		break;

	default:
		return -EINVAL;
	}

	wrpkru(pkru);
	return 0;
}
