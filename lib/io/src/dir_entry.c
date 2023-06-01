/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_entry.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "io/dir_entry.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

dir_entry_t *dir_entry_create(const char *name)
{
	size_t len, name_len;
	dir_entry_t *out;

	name_len = strlen(name);
	if (SZ_ADD_OV(name_len, 1, &name_len))
		return NULL;
	if (SZ_ADD_OV(sizeof(*out), name_len, &len))
		return NULL;

	out = calloc(1, len);
	if (out == NULL)
		return NULL;

	memcpy(out->name, name, name_len);
	return out;
}

dir_entry_xattr_t *dir_entry_xattr_create(const char *key, const sqfs_u8 *value,
					  size_t value_len)
{
	dir_entry_xattr_t *out;
	size_t len, key_len;

	/* key_ley = strlen(key) + 1 */
	key_len = strlen(key);
	if (SZ_ADD_OV(key_len, 1, &key_len))
		return NULL;

	/* len = key_len + value_len + 1 + sizeof(*out) */
	if (SZ_ADD_OV(key_len, value_len, &len))
		return NULL;
	if (SZ_ADD_OV(len, 1, &len))
		return NULL;
	if (SZ_ADD_OV(len, sizeof(*out), &len))
		return NULL;

	out = calloc(1, len);
	if (out != NULL) {
		out->key = out->data;
		out->value = (sqfs_u8 *)out->data + key_len;
		out->value_len = value_len;

		memcpy(out->key, key, key_len);
		memcpy(out->value, value, value_len);
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
