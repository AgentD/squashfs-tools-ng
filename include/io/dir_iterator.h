/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_iterator.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_DIR_ITERATOR_H
#define IO_DIR_ITERATOR_H

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

	sqfs_u64 dev;

	int (*next)(struct dir_iterator_t *it,
		    dir_entry_t **out);

	int (*read_link)(struct dir_iterator_t *it, char **out);

	int (*open_subdir)(struct dir_iterator_t *it,
			   struct dir_iterator_t **out);
} dir_iterator_t;

#ifdef __cplusplus
extern "C" {
#endif

dir_iterator_t *dir_iterator_create(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* IO_DIR_ITERATOR_H */
