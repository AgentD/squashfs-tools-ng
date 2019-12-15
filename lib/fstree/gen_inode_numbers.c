/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gen_inode_numbers.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <stdio.h>

static void map_child_nodes(fstree_t *fs, tree_node_t *root, size_t *counter)
{
	bool has_subdirs = false;
	tree_node_t *it;

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		if (S_ISDIR(it->mode)) {
			has_subdirs = true;
			break;
		}
	}

	if (has_subdirs) {
		for (it = root->data.dir.children; it != NULL; it = it->next) {
			if (S_ISDIR(it->mode))
				map_child_nodes(fs, it, counter);
		}
	}

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		fs->unique_inode_count += 1;

		it->inode_num = *counter;
		*counter += 1;
	}
}

void fstree_gen_inode_numbers(fstree_t *fs)
{
	size_t inum = 1;

	fs->unique_inode_count = 0;
	map_child_nodes(fs, fs->root, &inum);
	fs->root->inode_num = inum;
	fs->unique_inode_count += 1;
}
