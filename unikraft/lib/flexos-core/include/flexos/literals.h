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

#ifndef FLEXOS_LITERALS_H
#define FLEXOS_LITERALS_H

/*   Je ne sais pourquoi \ Mon esprit amer
 *   D'une aile inquiète et folle vole sur la mer.
 *
 * Beautiful. We have a problem with string literals. The compiler always stores
 * them in the library's .rodata section, meaning that it lands into .data_comp1
 * if the lib is isolated. Meaning that this call
 *
 *   flexos_intelpku_gate(1, 0, uk_pr_info, "Hello!");
 *
 * would crash even though gates are properly inserted. In a perfect world we'd
 * have a compiler pass that detects this and puts the literal in a shared
 * section. But "La vie est bien sévère" and we don't have time for this.
 *
 * De la douceur, de la douceur, de la douceur !
 *
 * Here is a macro that you can use to force the compiler to share the string
 * literal, e.g.:
 *
 *   flexos_intelpku_gate(1, 0, uk_pr_info, FLEXOS_SHARED_LITERAL("Hello!"));
 */
#define FLEXOS_SHARED_LITERAL(str) (						\
	{									\
		static char __attribute__((section(".data_shared")))		\
			__str[] = str; __str;					\
	}									\
)

#define FLEXOS_SHARED_INT(num) (						\
	{									\
		static int __attribute__((section(".data_shared")))		\
			__num = num; __num;					\
	}									\
)

#endif /* FLEXOS_LITERALS_H */
