/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_XATTR_H
#define IO_XATTR_H

#include "sqfs/predef.h"

typedef struct dir_entry_xattr_t {
	struct dir_entry_xattr_t *next;
	char *key;
	sqfs_u8 *value;
	size_t value_len;
	char data[];
} dir_entry_xattr_t;

#ifdef __cplusplus
extern "C" {
#endif

dir_entry_xattr_t *dir_entry_xattr_create(const char *key, const sqfs_u8 *value,
					  size_t value_len);

void dir_entry_xattr_list_free(dir_entry_xattr_t *list);

#ifdef __cplusplus
}
#endif

#endif /* IO_XATTR_H */
