/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr.h
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_XATTR_H
#define IO_XATTR_H

typedef struct dir_entry_xattr_t {
	struct dir_entry_xattr_t *next;
	char *key;
	sqfs_u8 *value;
	size_t value_len;
	char data[];
} dir_entry_xattr_t;

#endif /* IO_XATTR_H */
