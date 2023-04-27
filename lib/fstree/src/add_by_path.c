/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * add_by_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <string.h>
#include <assert.h>
#include <errno.h>

tree_node_t *fstree_add_generic(fstree_t *fs, const char *path,
				const struct stat *sb, const char *extra)
{
	return fstree_add_generic_at(fs, fs->root, path, sb, extra);
}

tree_node_t *fstree_add_generic_at(fstree_t *fs, tree_node_t *root,
				   const char *path, const struct stat *sb,
				   const char *extra)
{
	tree_node_t *child, *parent;
	const char *name;

	if (*path == '\0') {
		child = root;
		assert(child != NULL);
		goto out;
	}

	parent = fstree_get_node_by_path(fs, root, path, true, true);
	if (parent == NULL)
		return NULL;

	name = strrchr(path, '/');
	name = (name == NULL ? path : (name + 1));

	child = parent->data.children;
	while (child != NULL && strcmp(child->name, name) != 0)
		child = child->next;
out:
	if (child != NULL) {
		if (!S_ISDIR(child->mode) || !S_ISDIR(sb->st_mode) ||
		    !(child->flags & FLAG_DIR_CREATED_IMPLICITLY)) {
			errno = EEXIST;
			return NULL;
		}

		child->uid = sb->st_uid;
		child->gid = sb->st_gid;
		child->mode = sb->st_mode;
		child->mod_time = sb->st_mtime;
		child->flags &= ~FLAG_DIR_CREATED_IMPLICITLY;
		return child;
	}

	return fstree_mknode(parent, name, strlen(name), extra, sb);
}
