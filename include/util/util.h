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

#define container_of(ptr, type, member) \
	((type *)((char *)ptr - offsetof(type, member)))

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

#endif /* UTIL_H */
