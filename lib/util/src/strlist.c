/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * strlist.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/strlist.h"
#include "util/util.h"
#include "compat.h"

#include <stdlib.h>

void strlist_cleanup(strlist_t *list)
{
	for (size_t i = 0; i < list->count; ++i)
		free(list->strings[i]);

	free(list->strings);
	memset(list, 0, sizeof(*list));
}

int strlist_init_copy(strlist_t *dst, const strlist_t *src)
{
	memset(dst, 0, sizeof(*dst));

	dst->strings = alloc_array(sizeof(char *), src->capacity);
	if (dst->strings == NULL)
		return -1;

	dst->count = src->count;
	dst->capacity = src->capacity;

	for (size_t i = 0; i < src->count; ++i) {
		dst->strings[i] = strdup(src->strings[i]);

		if (dst->strings[i] == NULL) {
			dst->count = i;
			strlist_cleanup(dst);
			return -1;
		}
	}

	return 0;
}

int strlist_append(strlist_t *list, const char *str)
{
	if (list->count == list->capacity) {
		size_t new_cap = list->capacity ? (list->capacity * 2) : 128;
		size_t new_sz;
		char **new;

		if (SZ_MUL_OV(new_cap, sizeof(char *), &new_sz))
			return -1;

		new = realloc(list->strings, new_sz);
		if (new == NULL)
			return -1;

		list->capacity = new_cap;
		list->strings = new;
	}

	list->strings[list->count] = strdup(str);
	if (list->strings[list->count] == NULL)
		return -1;

	list->count += 1;
	return 0;
}
