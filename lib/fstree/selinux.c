/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <selinux/selinux.h>
#include <selinux/label.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define XATTR_NAME_SELINUX "security.selinux"
#define XATTR_VALUE_SELINUX "system_u:object_r:unlabeled_t:s0"

static char *get_path(tree_node_t *node)
{
	tree_node_t *it;
	char *str, *ptr;
	size_t len = 0;

	if (node->parent == NULL) {
		str = strdup("/");

		if (str == NULL)
			goto fail_alloc;
		return str;
	}

	for (it = node; it != NULL && it->parent != NULL; it = it->parent) {
		len += strlen(it->name) + 1;
	}

	str = malloc(len + 1);
	if (str == NULL)
		goto fail_alloc;

	ptr = str + len;
	*ptr = '\0';

	for (it = node; it != NULL && it->parent != NULL; it = it->parent) {
		len = strlen(it->name);
		ptr -= len;

		memcpy(ptr, it->name, len);
		*(--ptr) = '/';
	}

	return str;
fail_alloc:
	perror("relabeling files");
	return NULL;
}

static int relable_node(fstree_t *fs, struct selabel_handle *sehnd,
			tree_node_t *node)
{
	char *context = NULL, *path;
	tree_node_t *it;
	int ret;

	path = get_path(node);
	if (path == NULL)
		return -1;

	if (selabel_lookup(sehnd, &context, path, node->mode) < 0) {
		free(path);

		ret = fstree_add_xattr(fs, node, XATTR_NAME_SELINUX,
				       XATTR_VALUE_SELINUX);
	} else {
		free(path);
		ret = fstree_add_xattr(fs, node, XATTR_NAME_SELINUX, context);
		free(context);
	}

	if (ret)
		return -1;

	if (S_ISDIR(node->mode)) {
		it = node->data.dir->children;

		while (it != NULL) {
			if (relable_node(fs, sehnd, it))
				return -1;

			it = it->next;
		}
	}

	return 0;
}

int fstree_relabel_selinux(fstree_t *fs, const char *filename)
{
	struct selabel_handle *sehnd;
	struct selinux_opt seopts[] = {
		{ SELABEL_OPT_PATH, filename },
	};
	int ret;

	sehnd = selabel_open(SELABEL_CTX_FILE, seopts, 1);

	if (sehnd == NULL) {
		perror(filename);
		return -1;
	}

	ret = relable_node(fs, sehnd, fs->root);

	selabel_close(sehnd);
	return ret;
}
