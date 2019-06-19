/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>
#include <errno.h>

static tree_node_t *child_by_name(tree_node_t *root, const char *name,
				  size_t len)
{
	tree_node_t *n = root->data.dir->children;

	while (n != NULL) {
		if (strncmp(n->name, name, len) == 0 && n->name[len] == '\0')
			break;

		n = n->next;
	}

	return n;
}

static tree_node_t *get_parent_node(fstree_t *fs, tree_node_t *root,
				    const char *path)
{
	const char *end;
	tree_node_t *n;

	for (;;) {
		if (!S_ISDIR(root->mode)) {
			errno = ENOTDIR;
			return NULL;
		}

		end = strchr(path, '/');
		if (end == NULL)
			break;

		n = child_by_name(root, path, end - path);

		if (n == NULL) {
			n = fstree_mknode(fs, root, path, end - path, NULL,
					  &fs->defaults);
			if (n == NULL)
				return NULL;

			n->data.dir->created_implicitly = true;
		}

		root = n;
		path = end + 1;
	}

	return root;
}

tree_node_t *fstree_add_generic(fstree_t *fs, const char *path,
				const struct stat *sb, const char *extra)
{
	tree_node_t *child, *parent;
	const char *name;

	parent = get_parent_node(fs, fs->root, path);
	if (parent == NULL)
		return NULL;

	name = strrchr(path, '/');
	name = (name == NULL ? path : (name + 1));

	child = child_by_name(parent, name, strlen(name));
	if (child != NULL) {
		if (!S_ISDIR(child->mode) || !S_ISDIR(sb->st_mode) ||
		    !child->data.dir->created_implicitly) {
			errno = EEXIST;
			return NULL;
		}

		child->data.dir->created_implicitly = false;
		return child;
	}

	return fstree_mknode(fs, parent, name, strlen(name), extra, sb);
}
