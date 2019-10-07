/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_H
#define UTIL_H

#include "config.h"
#include "sqfs/predef.h"

#include <stdint.h>
#include <stddef.h>

#include "compat.h"

#if defined(__GNUC__) || defined(__clang__)
#define UI_ADD_OV __builtin_uadd_overflow
#define UL_ADD_OV __builtin_uaddl_overflow
#define ULL_ADD_OV __builtin_uaddll_overflow

#define UI_MUL_OV __builtin_umul_overflow
#define UL_MUL_OV __builtin_umull_overflow
#define ULL_MUL_OV __builtin_umulll_overflow
#else
#error Sorry, I do not know how to trap integer overflows with this compiler
#endif

#if SIZEOF_SIZE_T <= SIZEOF_INT
#define SZ_ADD_OV UI_ADD_OV
#define SZ_MUL_OV UI_MUL_OV
#elif SIZEOF_SIZE_T == SIZEOF_LONG
#define SZ_ADD_OV UL_ADD_OV
#define SZ_MUL_OV UL_MUL_OV
#elif SIZEOF_SIZE_T == SIZEOF_LONG_LONG
#define SZ_ADD_OV ULL_ADD_OV
#define SZ_MUL_OV ULL_MUL_OV
#else
#error Cannot determine maximum value of size_t
#endif

/*
  Helper for allocating data structures with flexible array members.

  'base_size' is the size of the struct itself, 'item_size' the size of a
  single array element and 'nmemb' the number of elements.

  Iternally checks for arithmetic overflows when allocating the combined thing.
 */
SQFS_INTERNAL
void *alloc_flex(size_t base_size, size_t item_size, size_t nmemb);

/* Basically the same as calloc, but *ALWAYS* does overflow checking */
SQFS_INTERNAL
void *alloc_array(size_t item_size, size_t nmemb);

/*
  Convert back to forward slashed, remove all preceeding and trailing slashes,
  collapse all sequences of slashes, remove all path components that are '.'
  and returns failure state if one of the path components is '..'.

  Returns 0 on success.
*/
SQFS_INTERNAL int canonicalize_name(char *filename);

#endif /* UTIL_H */
