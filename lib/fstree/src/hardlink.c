/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hardlink.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

static int resolve_link(fstree_t *fs, tree_node_t *node)
{
	tree_node_t *start = node;

	for (;;) {
		if (!S_ISLNK(node->mode) || !(node->flags & FLAG_LINK_IS_HARD))
			break;

		if (node->flags & FLAG_LINK_RESOVED) {
			node = node->data.target_node;
		} else {
			node = fstree_get_node_by_path(fs, fs->root,
						       node->data.target,
						       false, false);
			if (node == NULL)
				return -1;
		}

		if (node == start) {
			errno = EMLINK;
			return -1;
		}
	}

	if (S_ISDIR(node->mode)) {
		errno = EPERM;
		return -1;
	}

	if (node->link_count == 0xFFFFFFFF) {
		errno = EMLINK;
		return -1;
	}

	start->flags |= FLAG_LINK_RESOVED;
	start->data.target_node = node;

	node->link_count++;
	return 0;
}

int fstree_resolve_hard_links(fstree_t *fs)
{
	while (fs->links_unresolved != NULL) {
		tree_node_t *n = fs->links_unresolved;

		if (resolve_link(fs, n)) {
			char *path = fstree_get_path(n);
			fprintf(stderr,
				"Resolving hard link '%s' -> '%s': %s\n",
				path == NULL ? n->name : path, n->data.target,
				strerror(errno));
			free(path);
			return -1;
		}

		fs->links_unresolved = n->next_by_type;
		n->next_by_type = NULL;
	}

	return 0;
}
