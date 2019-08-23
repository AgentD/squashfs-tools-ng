/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

char *fstree_get_path(tree_node_t *node)
{
	tree_node_t *it;
	char *str, *ptr;
	size_t len = 0;

	if (node->parent == NULL)
		return strdup("/");

	for (it = node; it != NULL && it->parent != NULL; it = it->parent) {
		if (SZ_ADD_OV(len, strlen(it->name), &len) ||
		    SZ_ADD_OV(len, 1, &len))
			goto fail_ov;
	}

	if (SZ_ADD_OV(len, 1, &len))
		goto fail_ov;

	str = malloc(len);
	if (str == NULL)
		return NULL;

	ptr = str + len - 1;
	*ptr = '\0';

	for (it = node; it != NULL && it->parent != NULL; it = it->parent) {
		len = strlen(it->name);
		ptr -= len;

		memcpy(ptr, it->name, len);
		*(--ptr) = '/';
	}

	return str;
fail_ov:
	errno = EOVERFLOW;
	return NULL;
}
