/*
 * Copyright © 2010 Valve Software
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "sqfs/predef.h"

#include <assert.h>
#include <stdint.h>

/*
 * Code for fast 32-bit unsigned remainder, based off of "Faster Remainder by
 * Direct Computation: Applications to Compilers and Software Libraries,"
 * available at https://arxiv.org/pdf/1902.01961.pdf.
 *
 * util_fast_urem32(n, d, REMAINDER_MAGIC(d)) returns the same thing as
 * n % d for any unsigned n and d, however it compiles down to only a few
 * multiplications, so it should be faster than plain sqfs_u32 modulo if the
 * same divisor is used many times.
 */

#define REMAINDER_MAGIC(divisor) \
   ((sqfs_u64) ~0ull / (divisor) + 1)

/*
 * Get bits 64-96 of a 32x64-bit multiply. If __int128_t is available, we use
 * it, which usually compiles down to one instruction on 64-bit architectures.
 * Otherwise on 32-bit architectures we usually get four instructions (one
 * 32x32->64 multiply, one 32x32->32 multiply, and one 64-bit add).
 */

static inline sqfs_u32
_mul32by64_hi(sqfs_u32 a, sqfs_u64 b)
{
#if __SIZEOF_INT128__ == 16
   return ((__uint128_t) b * a) >> 64;
#else
   /*
    * Let b = b0 + 2^32 * b1. Then a * b = a * b0 + 2^32 * a * b1. We would
    * have to do a 96-bit addition to get the full result, except that only
    * one term has non-zero lower 32 bits, which means that to get the high 32
    * bits, we only have to add the high 64 bits of each term. Unfortunately,
    * we have to do the 64-bit addition in case the low 32 bits overflow.
    */
   sqfs_u32 b0 = (sqfs_u32) b;
   sqfs_u32 b1 = b >> 32;
   return ((((sqfs_u64) a * b0) >> 32) + (sqfs_u64) a * b1) >> 32;
#endif
}

static inline sqfs_u32
util_fast_urem32(sqfs_u32 n, sqfs_u32 d, sqfs_u64 magic)
{
   sqfs_u64 lowbits = magic * n;
   sqfs_u32 result = _mul32by64_hi(d, lowbits);
   assert(result == n % d);
   return result;
}

