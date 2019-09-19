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
	uint32_t index;
	size_t i;

	if (xattr == NULL)
		return 0;

	switch (inode->base.type) {
	case SQFS_INODE_EXT_DIR:
		index = inode->data.dir_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FILE:
		index = inode->data.file_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_SLINK:
		index = inode->data.slink_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		index = inode->data.dev_ext.xattr_idx;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		index = inode->data.ipc_ext.xattr_idx;
		break;
	default:
		return 0;
	}

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
