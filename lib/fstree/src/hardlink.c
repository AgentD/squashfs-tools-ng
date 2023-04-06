/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hardlink.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/util.h"
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

static int resolve_link(fstree_t *fs, tree_node_t *node)
{
	tree_node_t *start = node;

	while (node->mode == FSTREE_MODE_HARD_LINK ||
	       node->mode == FSTREE_MODE_HARD_LINK_RESOLVED) {
		if (node->mode == FSTREE_MODE_HARD_LINK_RESOLVED) {
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

	start->mode = FSTREE_MODE_HARD_LINK_RESOLVED;
	start->data.target_node = node;

	node->link_count++;
	return 0;
}

static int resolve_hard_links_dfs(fstree_t *fs, tree_node_t *n)
{
	tree_node_t *it;

	if (n->mode == FSTREE_MODE_HARD_LINK) {
		if (resolve_link(fs, n))
			goto fail_link;

		assert(n->mode == FSTREE_MODE_HARD_LINK_RESOLVED);
		it = n->data.target_node;

		if (S_ISDIR(it->mode) && it->data.dir.visited)
			goto fail_link_loop;
	} else if (S_ISDIR(n->mode)) {
		n->data.dir.visited = true;

		for (it = n->data.dir.children; it != NULL; it = it->next) {
			if (resolve_hard_links_dfs(fs, it))
				return -1;
		}

		n->data.dir.visited = false;
	}

	return 0;
fail_link: {
	char *path = fstree_get_path(n);
	fprintf(stderr, "Resolving hard link '%s' -> '%s': %s\n",
		path == NULL ? n->name : path, n->data.target,
		strerror(errno));
	free(path);
}
	return -1;
fail_link_loop: {
	char *npath = fstree_get_path(n);
	char *tpath = fstree_get_path(it);
	fprintf(stderr, "Hard link loop detected in '%s' -> '%s'\n",
		npath == NULL ? n->name : npath,
		tpath == NULL ? it->name : tpath);
	free(npath);
	free(tpath);
}
	return -1;
}

tree_node_t *fstree_add_hard_link(fstree_t *fs, const char *path,
				  const char *target)
{
	struct stat sb;
	tree_node_t *n;

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFLNK | 0777;

	n = fstree_add_generic(fs, path, &sb, target);
	if (n != NULL) {
		if (canonicalize_name(n->data.target)) {
			free(n);
			errno = EINVAL;
			return NULL;
		}

		n->mode = FSTREE_MODE_HARD_LINK;
	}

	return n;
}

int fstree_resolve_hard_links(fstree_t *fs)
{
	return resolve_hard_links_dfs(fs, fs->root);
}
