/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_iterator.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_DIR_ITERATOR_H
#define UTIL_DIR_ITERATOR_H

#include "sqfs/predef.h"

typedef struct {
	sqfs_s64 mtime;
	sqfs_u64 dev;
	sqfs_u64 rdev;
	sqfs_u32 uid;
	sqfs_u32 gid;
	sqfs_u16 mode;

	char name[];
} dir_entry_t;

typedef struct dir_iterator_t {
	sqfs_object_t obj;

	int (*next)(struct dir_iterator_t *it,
		    dir_entry_t **out);
} dir_iterator_t;

dir_iterator_t *dir_iterator_create(const char *path);

#endif /* UTIL_DIR_ITERATOR_H */
