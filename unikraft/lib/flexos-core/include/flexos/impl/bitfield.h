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

#ifndef FLEXOS_VMEPT_BITFIELD_H
#define FLEXOS_VMEPT_BITFIELD_H

#include <stdint.h>

/* a bitfield containing 256 bits */
struct flexos_vmept_bitfield_256 {
	uint64_t eightbytes[4]; 
};

/* returns the smalles integer i such that eightbytes & (1 << i) == 0
 * or -1 if eightbytes is 0  
 * TODO: maybe use specific instructions to do this faster? */
static inline int _flexos_vmept_find_first_zero_bit_64(uint64_t eightbytes) {
	/* x & (x - 1) clears the rightmost set bit 
	 * thus x - (x & (x - 1)) only keeps the righmost set bit (if any) */
	uint64_t x = ~eightbytes;
	x = x - (x & (x - 1));
	
	switch (x) {
		case (1ULL << 0): return 0;
		case (1ULL << 1): return 1;
		case (1ULL << 2): return 2;
		case (1ULL << 3): return 3;
		case (1ULL << 4): return 4;
		case (1ULL << 5): return 5;
		case (1ULL << 6): return 6;
		case (1ULL << 7): return 7;
		case (1ULL << 8): return 8;
		case (1ULL << 9): return 9;
		case (1ULL << 10): return 10;
		case (1ULL << 11): return 11;
		case (1ULL << 12): return 12;
		case (1ULL << 13): return 13;
		case (1ULL << 14): return 14;
		case (1ULL << 15): return 15;
		case (1ULL << 16): return 16;
		case (1ULL << 17): return 17;
		case (1ULL << 18): return 18;
		case (1ULL << 19): return 19;
		case (1ULL << 20): return 20;
		case (1ULL << 21): return 21;
		case (1ULL << 22): return 22;
		case (1ULL << 23): return 23;
		case (1ULL << 24): return 24;
		case (1ULL << 25): return 25;
		case (1ULL << 26): return 26;
		case (1ULL << 27): return 27;
		case (1ULL << 28): return 28;
		case (1ULL << 29): return 29;
		case (1ULL << 30): return 30;
		case (1ULL << 31): return 31;
		case (1ULL << 32): return 32;
		case (1ULL << 33): return 33;
		case (1ULL << 34): return 34;
		case (1ULL << 35): return 35;
		case (1ULL << 36): return 36;
		case (1ULL << 37): return 37;
		case (1ULL << 38): return 38;
		case (1ULL << 39): return 39;
		case (1ULL << 40): return 40;
		case (1ULL << 41): return 41;
		case (1ULL << 42): return 42;
		case (1ULL << 43): return 43;
		case (1ULL << 44): return 44;
		case (1ULL << 45): return 45;
		case (1ULL << 46): return 46;
		case (1ULL << 47): return 47;
		case (1ULL << 48): return 48;
		case (1ULL << 49): return 49;
		case (1ULL << 50): return 50;
		case (1ULL << 51): return 51;
		case (1ULL << 52): return 52;
		case (1ULL << 53): return 53;
		case (1ULL << 54): return 54;
		case (1ULL << 55): return 55;
		case (1ULL << 56): return 56;
		case (1ULL << 57): return 57;
		case (1ULL << 58): return 58;
		case (1ULL << 59): return 59;
		case (1ULL << 60): return 60;
		case (1ULL << 61): return 61;
		case (1ULL << 62): return 62;
		case (1ULL << 63): return 63;
		default: return -1;
	}
}

static inline int flexos_vmept_has_zero_bit_256(const struct flexos_vmept_bitfield_256 *bf_256) {
	return (~bf_256->eightbytes[0]) || (~bf_256->eightbytes[1]) 
		|| (~bf_256->eightbytes[2]) || (~bf_256->eightbytes[3]);
}

static inline int flexos_vmept_find_first_zero_bit_256(const struct flexos_vmept_bitfield_256 *bf_256) {
	if (~bf_256->eightbytes[0]) 
		return _flexos_vmept_find_first_zero_bit_64(bf_256->eightbytes[0]);
	if (~bf_256->eightbytes[1]) 
		return 64 + _flexos_vmept_find_first_zero_bit_64(bf_256->eightbytes[1]);
	if (~bf_256->eightbytes[2]) 
		return 128 + _flexos_vmept_find_first_zero_bit_64(bf_256->eightbytes[2]);
	if (~bf_256->eightbytes[3]) 
		return 192 + _flexos_vmept_find_first_zero_bit_64(bf_256->eightbytes[3]);
	return -1;
}

static inline void flexos_vmept_set_bit_256(struct flexos_vmept_bitfield_256 *bf_256, uint8_t i) {
	bf_256->eightbytes[i / 64] |= 1ULL << (i % 64);
}

static inline void flexos_vmept_clear_bit_256(struct flexos_vmept_bitfield_256 *bf_256, uint8_t i) {
	bf_256->eightbytes[i / 64] &= ~(1ULL << (i % 64));
}

static inline void flexos_vmept_init_bitfield_256(struct flexos_vmept_bitfield_256 *bf_256) {
	bf_256->eightbytes[0] = 0ULL;
	bf_256->eightbytes[1] = 0ULL;
	bf_256->eightbytes[2] = 0ULL;
	bf_256->eightbytes[3] = 0ULL;
}

#endif /* FLEXOS_VMEPT_BITFIELD_H */
