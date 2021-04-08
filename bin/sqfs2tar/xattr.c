/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfs2tar.h"

static tar_xattr_t *mkxattr(const sqfs_xattr_entry_t *key,
			    const sqfs_xattr_value_t *value)
{
	tar_xattr_t *ent;

	ent = calloc(1, sizeof(*ent) + strlen((const char *)key->key) +
		     value->size + 2);

	if (ent == NULL) {
		perror("creating xattr entry");
		return NULL;
	}

	ent->key = ent->data;
	ent->value = (sqfs_u8 *)ent->key + strlen((const char *)key->key) + 1;
	ent->value_len = value->size;

	strcpy(ent->key, (const char *)key->key);
	memcpy(ent->value, value->value, value->size + 1);
	return ent;
}

int get_xattrs(const char *name, const sqfs_inode_generic_t *inode,
	       tar_xattr_t **out)
{
	tar_xattr_t *list = NULL, *ent;
	sqfs_xattr_value_t *value;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_id_t desc;
	sqfs_u32 index;
	size_t i;
	int ret;

	if (xr == NULL)
		return 0;

	sqfs_inode_get_xattr_index(inode, &index);
	if (index == 0xFFFFFFFF)
		return 0;

	ret = sqfs_xattr_reader_get_desc(xr, index, &desc);
	if (ret) {
		sqfs_perror(name, "resolving xattr index", ret);
		return -1;
	}

	ret = sqfs_xattr_reader_seek_kv(xr, &desc);
	if (ret) {
		sqfs_perror(name, "locating xattr key-value pairs", ret);
		return -1;
	}

	for (i = 0; i < desc.count; ++i) {
		ret = sqfs_xattr_reader_read_key(xr, &key);
		if (ret) {
			sqfs_perror(name, "reading xattr key", ret);
			goto fail;
		}

		ret = sqfs_xattr_reader_read_value(xr, key, &value);
		if (ret) {
			sqfs_perror(name, "reading xattr value", ret);
			sqfs_free(key);
			goto fail;
		}

		ent = mkxattr(key, value);
		sqfs_free(key);
		sqfs_free(value);

		if (ent == NULL)
			goto fail;

		ent->next = list;
		list = ent;
	}

	*out = list;
	return 0;
fail:
	free_xattr_list(list);
	return -1;
}
