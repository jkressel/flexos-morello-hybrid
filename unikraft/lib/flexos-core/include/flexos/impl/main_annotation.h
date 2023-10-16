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

#ifndef MAIN_ANNOTATION_H
#define MAIN_ANNOTATION_H

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

#if (FLEXOS_VMEPT_APPCOMP) == (FLEXOS_VMEPT_COMP_ID)
#define FLEXOS_VMEPT_MAIN_ANNOTATION __attribute__ ((section (".text_comp_exclusive")))
#else
#define FLEXOS_VMEPT_MAIN_ANNOTATION __attribute__ ((section ("/DISCARD/")))
#endif

int main(int argc, char *argv[]) FLEXOS_VMEPT_MAIN_ANNOTATION;

#endif /* MAIN_ANNOTATION_H */ 
