/*
 * xxHash - Extremely Fast Hash algorithm
 * Copyright (C) 2012-2016, Yann Collet.
 *
 * BSD 2-Clause License (http://www.opensource.org/licenses/bsd-license.php)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ----------------------------------------------------------------------------
 * This code has been lifted from Linux (the OS kernel) and adapted for use in
 * libsquashfs. For the original, unmodified and complete source, see below.
 *
 * You can contact the author at:
 * - xxHash homepage: http://cyan4973.github.io/xxHash/
 * - xxHash source repository: https://github.com/Cyan4973/xxHash
 */
#include "config.h"
#include "util.h"

#include <string.h>

#define xxh_rotl32(x, r) ((x << r) | (x >> (32 - r)))

static const sqfs_u32 PRIME32_1 = 2654435761U;
static const sqfs_u32 PRIME32_2 = 2246822519U;
static const sqfs_u32 PRIME32_3 = 3266489917U;
static const sqfs_u32 PRIME32_4 =  668265263U;
static const sqfs_u32 PRIME32_5 =  374761393U;

static sqfs_u32 xxh32_round(sqfs_u32 seed, sqfs_u32 input)
{
	seed += input * PRIME32_2;
	seed = xxh_rotl32(seed, 13);
	seed *= PRIME32_1;
	return seed;
}

static sqfs_u32 XXH_readLE32(const sqfs_u8 *ptr)
{
	sqfs_u32 value;
	memcpy(&value, ptr, sizeof(value));
	return le32toh(value);
}

sqfs_u32 xxh32(const void *input, const size_t len)
{
	const sqfs_u8 *p = (const sqfs_u8 *)input;
	const sqfs_u8 *b_end = p + len;
	sqfs_u32 h32;

	if (len >= 16) {
		const sqfs_u8 *const limit = b_end - 16;
		sqfs_u32 v1 = PRIME32_1 + PRIME32_2;
		sqfs_u32 v2 = PRIME32_2;
		sqfs_u32 v3 = 0;
		sqfs_u32 v4 = PRIME32_1;

		do {
			v1 = xxh32_round(v1, XXH_readLE32(p     ));
			v2 = xxh32_round(v2, XXH_readLE32(p +  4));
			v3 = xxh32_round(v3, XXH_readLE32(p +  8));
			v4 = xxh32_round(v4, XXH_readLE32(p + 12));
			p += 16;
		} while (p <= limit);

		h32 = xxh_rotl32(v1, 1) + xxh_rotl32(v2, 7) +
			xxh_rotl32(v3, 12) + xxh_rotl32(v4, 18);
	} else {
		h32 = PRIME32_5;
	}

	h32 += (sqfs_u32)len;

	while (p + 4 <= b_end) {
		h32 += XXH_readLE32(p) * PRIME32_3;
		h32 = xxh_rotl32(h32, 17) * PRIME32_4;
		p += 4;
	}

	while (p < b_end) {
		h32 += (*p) * PRIME32_5;
		h32 = xxh_rotl32(h32, 11) * PRIME32_1;
		p++;
	}

	h32 ^= h32 >> 15;
	h32 *= PRIME32_2;
	h32 ^= h32 >> 13;
	h32 *= PRIME32_3;
	h32 ^= h32 >> 16;
	return h32;
}
