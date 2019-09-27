/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dump_xattrs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

int dump_xattrs(sqfs_xattr_reader_t *xattr, const sqfs_inode_generic_t *inode)
{
	sqfs_xattr_value_t *value;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_id_t desc;
	sqfs_u32 index;
	size_t i;

	if (xattr == NULL)
		return 0;

	sqfs_inode_get_xattr_index(inode, &index);

	if (index == 0xFFFFFFFF)
		return 0;

	if (sqfs_xattr_reader_get_desc(xattr, index, &desc)) {
		fputs("Error resolving xattr index\n", stderr);
		return -1;
	}

	if (sqfs_xattr_reader_seek_kv(xattr, &desc)) {
		fputs("Error locating xattr key-value pairs\n", stderr);
		return -1;
	}

	for (i = 0; i < desc.count; ++i) {
		if (sqfs_xattr_reader_read_key(xattr, &key)) {
			fputs("Error reading xattr key\n", stderr);
			return -1;
		}

		if (sqfs_xattr_reader_read_value(xattr, key, &value)) {
			fputs("Error reading xattr value\n", stderr);
			free(key);
			return -1;
		}

		printf("%s=%s\n", key->key, value->value);
		free(key);
		free(value);
	}

	return 0;
}
