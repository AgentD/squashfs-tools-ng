/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <selinux/selinux.h>
#include <selinux/label.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define XATTR_NAME_SELINUX "security.selinux"
#define XATTR_VALUE_SELINUX "system_u:object_r:unlabeled_t:s0"

static int relable_node(fstree_t *fs, struct selabel_handle *sehnd,
			tree_node_t *node)
{
	char *context = NULL, *path;
	int ret;

	path = fstree_get_path(node);
	if (path == NULL)
		goto fail;

	if (selabel_lookup(sehnd, &context, path, node->mode) < 0) {
		context = strdup(XATTR_VALUE_SELINUX);
		if (context == NULL)
			goto fail;
	}

	ret = fstree_add_xattr(fs, node, XATTR_NAME_SELINUX, context);
	free(context);
	free(path);
	return ret;
fail:
	perror("relabeling files");
	free(path);
	return -1;
}

int fstree_relabel_selinux(fstree_t *fs, const char *filename)
{
	struct selabel_handle *sehnd;
	struct selinux_opt seopts[] = {
		{ SELABEL_OPT_PATH, filename },
	};
	size_t i;
	int ret = 0;

	sehnd = selabel_open(SELABEL_CTX_FILE, seopts, 1);

	if (sehnd == NULL) {
		perror(filename);
		return -1;
	}

	for (i = 2; i < fs->inode_tbl_size; ++i) {
		ret = relable_node(fs, sehnd, fs->inode_table[i]);
		if (ret)
			break;
	}

	selabel_close(sehnd);
	return ret;
}
