/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xattr.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "io/xattr.h"

#include <stdlib.h>
#include <string.h>

dir_entry_xattr_t *dir_entry_xattr_create(const char *key, const sqfs_u8 *value,
					  size_t value_len)
{
	size_t key_len = strlen(key);
	dir_entry_xattr_t *out = calloc(1, sizeof(*out) + key_len + 1 +
					value_len + 1);

	if (out != NULL) {
		out->key = out->data;
		out->value = (sqfs_u8 *)(out->data + key_len + 1);

		memcpy(out->key, key, key_len);
		memcpy(out->value, value, value_len);
		out->value_len = value_len;
	}

	return out;
}

dir_entry_xattr_t *dir_entry_xattr_list_copy(const dir_entry_xattr_t *list)
{
	dir_entry_xattr_t *new, *copy = NULL, *copy_last = NULL;
	const dir_entry_xattr_t *it;

	for (it = list; it != NULL; it = it->next) {
		new = dir_entry_xattr_create(it->key, it->value,
					     it->value_len);
		if (new == NULL) {
			dir_entry_xattr_list_free(copy);
			return NULL;
		}

		if (copy_last == NULL) {
			copy = new;
			copy_last = new;
		} else {
			copy_last->next = new;
			copy_last = new;
		}
	}

	return copy;
}

void dir_entry_xattr_list_free(dir_entry_xattr_t *list)
{
	dir_entry_xattr_t *old;

	while (list != NULL) {
		old = list;
		list = list->next;
		free(old);
	}
}
