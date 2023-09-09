/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * dir_tree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <string.h>
#include <stdlib.h>

void sqfs_dir_tree_destroy(sqfs_tree_node_t *root)
{
	sqfs_tree_node_t *it;

	if (!root)
		return;

	while (root->children != NULL) {
		it = root->children;
		root->children = it->next;

		sqfs_dir_tree_destroy(it);
	}

	free(root->inode);
	free(root);
}

int sqfs_tree_node_get_path(const sqfs_tree_node_t *node, char **out)
{
	const sqfs_tree_node_t *it;
	size_t clen, len = 0;
	char *str, *ptr;

	*out = NULL;

	if (node == NULL)
		return SQFS_ERROR_ARG_INVALID;

	for (it = node; it->parent != NULL; it = it->parent) {
		if (it->parent == node)
			return SQFS_ERROR_LINK_LOOP;

		/* non-root nodes must have a valid name */
		clen = strlen((const char *)it->name);

		if (clen == 0)
			return SQFS_ERROR_CORRUPTED;

		if (strchr((const char *)it->name, '/') != NULL)
			return SQFS_ERROR_CORRUPTED;

		if (it->name[0] == '.') {
			if (clen == 1 || (clen == 2 && it->name[1] == '.'))
				return SQFS_ERROR_CORRUPTED;
		}

		/* compute total path length */
		if (SZ_ADD_OV(clen, 1, &clen))
			return SQFS_ERROR_OVERFLOW;

		if (SZ_ADD_OV(len, clen, &len))
			return SQFS_ERROR_OVERFLOW;
	}

	/* root node must not have a name */
	if (it->name[0] != '\0')
		return SQFS_ERROR_ARG_INVALID;

	/* generate the path */
	if (node->parent == NULL) {
		str = strdup("/");
		if (str == NULL)
			return SQFS_ERROR_ALLOC;
	} else {
		if (SZ_ADD_OV(len, 1, &len))
			return SQFS_ERROR_OVERFLOW;

		str = malloc(len);
		if (str == NULL)
			return SQFS_ERROR_ALLOC;

		ptr = str + len - 1;
		*ptr = '\0';

		for (it = node; it->parent != NULL; it = it->parent) {
			len = strlen((const char *)it->name);
			ptr -= len;

			memcpy(ptr, (const char *)it->name, len);
			*(--ptr) = '/';
		}
	}

	*out = str;
	return 0;
}
