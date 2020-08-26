/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <string.h>
#include <stdlib.h>

char *sqfs_tree_node_get_path(const sqfs_tree_node_t *node)
{
	const sqfs_tree_node_t *it;
	char *str, *ptr;
	size_t len = 0;

	if (node->parent == NULL) {
		if (node->name[0] != '\0')
			return strdup((const char *)node->name);

		return strdup("/");
	}

	for (it = node; it != NULL && it->parent != NULL; it = it->parent) {
		len += strlen((const char *)it->name) + 1;
	}

	str = malloc(len + 1);
	if (str == NULL)
		return NULL;

	ptr = str + len;
	*ptr = '\0';

	for (it = node; it != NULL && it->parent != NULL; it = it->parent) {
		len = strlen((const char *)it->name);
		ptr -= len;

		memcpy(ptr, (const char *)it->name, len);
		*(--ptr) = '/';
	}

	return str;
}
