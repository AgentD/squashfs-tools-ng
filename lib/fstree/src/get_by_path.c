/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_by_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <string.h>
#include <errno.h>

static tree_node_t *child_by_name(tree_node_t *root, const char *name,
				  size_t len)
{
	tree_node_t *n = root->data.children;

	while (n != NULL) {
		if (strncmp(n->name, name, len) == 0 && n->name[len] == '\0')
			break;

		n = n->next;
	}

	return n;
}

tree_node_t *fstree_get_node_by_path(fstree_t *fs, tree_node_t *root,
				     const char *path, bool create_implicitly,
				     bool stop_at_parent)
{
	const char *end;
	tree_node_t *n;
	size_t len;

	while (*path != '\0') {
		while (*path == '/')
			++path;

		if (!S_ISDIR(root->mode)) {
			errno = ENOTDIR;
			return NULL;
		}

		end = strchr(path, '/');
		if (end == NULL) {
			if (stop_at_parent)
				break;

			len = strlen(path);
		} else {
			len = end - path;
		}

		n = child_by_name(root, path, len);

		if (n == NULL) {
			struct stat sb;

			if (!create_implicitly) {
				errno = ENOENT;
				return NULL;
			}

			memset(&sb, 0, sizeof(sb));
			sb.st_mode = S_IFDIR | (fs->defaults.mode & 07777);
			sb.st_uid = fs->defaults.uid;
			sb.st_gid = fs->defaults.gid;
			sb.st_mtime = fs->defaults.mtime;

			n = fstree_mknode(root, path, len, NULL, &sb);
			if (n == NULL)
				return NULL;

			n->flags |= FLAG_DIR_CREATED_IMPLICITLY;
		}

		root = n;
		path = path + len;
	}

	return root;
}
