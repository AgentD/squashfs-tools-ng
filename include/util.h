/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * util.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_UTIL_H
#define SQFS_UTIL_H

#include "config.h"
#include "sqfs/predef.h"
#include "compat.h"

#include <stddef.h>

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

SQFS_INTERNAL sqfs_u32 xxh32(const void *input, const size_t len);

/*
  Returns true if the given region of memory is filled with zero-bytes only.
 */
SQFS_INTERNAL bool is_memory_zero(const void *blob, size_t size);

#endif /* SQFS_UTIL_H */
