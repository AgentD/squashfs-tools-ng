/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hardlink.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

static size_t count_nodes(const sqfs_tree_node_t *n)
{
	size_t count = n->children == NULL ? 1 : 0;

	for (n = n->children; n != NULL; n = n->next)
		count += count_nodes(n);

	return count;
}

static size_t map_nodes(const sqfs_tree_node_t **list,
			const sqfs_tree_node_t *n,
			size_t idx)
{
	if (n->children == NULL)
		list[idx++] = n;

	for (n = n->children; n != NULL; n = n->next)
		idx = map_nodes(list, n, idx);

	return idx;
}

int sqfs_tree_find_hard_links(const sqfs_tree_node_t *root,
			      sqfs_hard_link_t **out)
{
	sqfs_hard_link_t *hardlinks = NULL, *lnk = NULL;
	const sqfs_tree_node_t **list = NULL;
	size_t i, j, count;
	const char *name;
	bool is_dup;

	count = count_nodes(root);
	list = malloc(sizeof(list[0]) * count);
	if (list == NULL)
		goto fail;

	map_nodes(list, root, 0);

	for (i = 0; i < count; ++i) {
		name = (const char *)list[i]->name;
		if (!is_filename_sane(name, false))
			continue;

		is_dup = false;

		for (j = 0; j < i; ++j) {
			if (list[j]->inode->base.inode_number ==
			    list[i]->inode->base.inode_number) {
				name = (const char *)list[j]->name;
				if (!is_filename_sane(name, false))
					continue;

				is_dup = true;
				break;
			}
		}

		if (is_dup) {
			lnk = calloc(1, sizeof(*lnk));
			if (lnk == NULL)
				goto fail;

			lnk->inode_number = list[j]->inode->base.inode_number;
			lnk->target = sqfs_tree_node_get_path(list[j]);
			if (lnk->target == NULL)
				goto fail;

			if (canonicalize_name(lnk->target) == 0) {
				lnk->next = hardlinks;
				hardlinks = lnk;
			} else {
				free(lnk->target);
				free(lnk);
			}
		}
	}

	*out = hardlinks;
	free(list);
	return 0;
fail:
	perror("detecting hard links in file system tree");
	free(lnk);
	while (hardlinks != NULL) {
		lnk = hardlinks;
		hardlinks = hardlinks->next;
		free(lnk->target);
		free(lnk);
	}
	free(list);
	return -1;
}
