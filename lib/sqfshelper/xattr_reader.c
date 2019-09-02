/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int restore_kv_pairs(xattr_reader_t *xr, fstree_t *fs,
			    tree_node_t *node)
{
	size_t i, key_idx, val_idx;
	sqfs_xattr_entry_t *key;
	sqfs_xattr_value_t *val;
	int ret;

	for (i = 0; i < node->xattr->num_attr; ++i) {
		key = xattr_reader_read_key(xr);
		if (key == NULL)
			return -1;

		val = xattr_reader_read_value(xr, key);
		if (val == NULL)
			goto fail_key;

		ret = str_table_get_index(&fs->xattr_keys,
					  (const char *)key->key, &key_idx);
		if (ret)
			goto fail_kv;

		ret = str_table_get_index(&fs->xattr_values,
					  (const char *)val->value, &val_idx);
		if (ret)
			goto fail_kv;

		if (sizeof(size_t) > sizeof(uint32_t)) {
			if (key_idx > 0xFFFFFFFFUL) {
				fputs("too many unique xattr keys\n", stderr);
				goto fail_kv;
			}

			if (val_idx > 0xFFFFFFFFUL) {
				fputs("too many unique xattr values\n", stderr);
				goto fail_kv;
			}
		}

		node->xattr->attr[i].key_index = key_idx;
		node->xattr->attr[i].value_index = val_idx;

		free(key);
		free(val);
	}

	return 0;
fail_kv:
	free(val);
fail_key:
	free(key);
	return -1;
}

int xattr_reader_restore_node(xattr_reader_t *xr, fstree_t *fs,
			      tree_node_t *node, uint32_t xattr)
{
	sqfs_xattr_id_t desc;
	tree_xattr_t *it;

	for (it = fs->xattr; it != NULL; it = it->next) {
		if (it->index == xattr) {
			node->xattr = it;
			return 0;
		}
	}

	if (xattr_reader_get_desc(xr, xattr, &desc))
		return -1;

	if (desc.count == 0 || desc.size == 0)
		return 0;

	node->xattr = alloc_flex(sizeof(*node->xattr),
				 sizeof(node->xattr->attr[0]), desc.count);
	if (node->xattr == NULL) {
		perror("creating xattr structure");
		return -1;
	}

	node->xattr->num_attr = desc.count;
	node->xattr->max_attr = desc.count;
	node->xattr->size = desc.size;
	node->xattr->index = xattr;
	node->xattr->owner = node;

	if (xattr_reader_seek_kv(xr, &desc))
		return -1;

	if (restore_kv_pairs(xr, fs, node)) {
		free(node->xattr);
		return -1;
	}

	node->xattr->next = fs->xattr;
	fs->xattr = node->xattr;
	return 0;
}
