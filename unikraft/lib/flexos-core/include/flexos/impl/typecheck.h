/* SPDX-License-Identifier: BSD-3-Clause */
/*
 * Copyright (c) 2021, Sebastian Rauch <s.rauch94@gmail.com>
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

#ifndef FLEXOS_TYPECHECK_H
#define FLEXOS_TYPECHECK_H

/* integer types at most 8 bytes long (excluding pointers) */
#define FLEXOS_TYPECLASS_INTEGER 1

/* floating point types at most 8 bytes long */
#define FLEXOS_TYPECLASS_SSE 2

/* floating point types longer than 8 bytes */
#define FLEXOS_TYPECLASS_SSE_EX 3

/* can be a pointer or a composite/aggregate type */
#define FLEXOS_TYPECLASS_UNKNOWN 4

#define _flexos_typeclass_of(x) _Generic((x),	 			\
		  _Bool : FLEXOS_TYPECLASS_INTEGER,			\
	    signed char : FLEXOS_TYPECLASS_INTEGER,			\
	  unsigned char : FLEXOS_TYPECLASS_INTEGER,			\
		  short : FLEXOS_TYPECLASS_INTEGER,			\
	 unsigned short : FLEXOS_TYPECLASS_INTEGER,			\
		    int : FLEXOS_TYPECLASS_INTEGER,			\
	   unsigned int : FLEXOS_TYPECLASS_INTEGER,			\
		   long : FLEXOS_TYPECLASS_INTEGER, 			\
	  unsigned long : FLEXOS_TYPECLASS_INTEGER,			\
	      long long : FLEXOS_TYPECLASS_INTEGER,			\
     unsigned long long : FLEXOS_TYPECLASS_INTEGER,			\
		  float : FLEXOS_TYPECLASS_SSE,				\
		 double : FLEXOS_TYPECLASS_SSE,				\
	    long double : FLEXOS_TYPECLASS_SSE_EX,			\
      		default : FLEXOS_TYPECLASS_UNKNOWN)				

/* TYPECLASS_UNKNOWN of size sizeof(void*) is assumed 
 * to be a pointer. Note that this might be incorrect
 * since any struct of that size also is classified as such.
 * When used only via flexos_heuristic_typeclass_of, no such
 * miscalssification should happen because structs cause errors in
 * _force_array_decay_or_error. */
#define _felxos_classify(tc, s)						\
(((tc) == FLEXOS_TYPECLASS_INTEGER) || ((tc) == FLEXOS_TYPECLASS_SSE) ||\
((tc) == FLEXOS_TYPECLASS_SSE_EX)) ? (tc) : ((s) == sizeof(void*) ?	\
FLEXOS_TYPECLASS_INTEGER : FLEXOS_TYPECLASS_UNKNOWN)

/* forces arrays to decay to pointers, 
 * doesn't change basic types (char, int, float, pointers, ...)
 * and results in an error for structs  */
#define _force_array_decay_or_error(x) ((x) + 0)

/* Note: if this causes an error, this might indicate that some 
 * unsupported type (e.g. a struct) was passed as argument to a gate  */
#define flexos_heuristic_typeclass_of(x) 				\
_felxos_classify(_flexos_typeclass_of(_force_array_decay_or_error(x)),  \
sizeof(_force_array_decay_or_error(x))) 

#define flexos_is_heuristic_typeclass_integer(x)			\
(flexos_heuristic_typeclass_of(x) == FLEXOS_TYPECLASS_INTEGER)

#endif
