/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void free_recursive(tree_node_t *n)
{
	tree_node_t *it;

	if (S_ISDIR(n->mode)) {
		while (n->data.children != NULL) {
			it = n->data.children;
			n->data.children = it->next;

			free_recursive(it);
		}
	}

	free(n);
}

int fstree_init(fstree_t *fs, const fstree_defaults_t *defaults)
{
	struct stat sb;

	memset(fs, 0, sizeof(*fs));
	fs->defaults = *defaults;

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | (defaults->mode & 07777);
	sb.st_uid = defaults->uid;
	sb.st_gid = defaults->gid;
	sb.st_mtime = defaults->mtime;

	fs->root = fstree_mknode(NULL, "", 0, NULL, &sb);

	if (fs->root == NULL) {
		perror("initializing file system tree");
		return -1;
	}

	fs->root->flags |= FLAG_DIR_CREATED_IMPLICITLY;
	return 0;
}

void fstree_cleanup(fstree_t *fs)
{
	free_recursive(fs->root);
	free(fs->inodes);
	memset(fs, 0, sizeof(*fs));
}
