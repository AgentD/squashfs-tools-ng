/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gen_inode_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

static size_t count_nodes(tree_node_t *root)
{
	tree_node_t *n = root->data.dir.children;
	size_t count = 1;

	while (n != NULL) {
		if (S_ISDIR(n->mode)) {
			count += count_nodes(n);
		} else {
			++count;
		}
		n = n->next;
	}

	return count;
}

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
		it->inode_num = *counter;
		*counter += 1;

		fs->inode_table[it->inode_num] = it;
	}
}

int fstree_gen_inode_table(fstree_t *fs)
{
	size_t inum = 2;

	fs->inode_tbl_size = count_nodes(fs->root) + 2;
	fs->inode_table = alloc_array(sizeof(tree_node_t *),
				      fs->inode_tbl_size);

	if (fs->inode_table == NULL) {
		perror("allocating inode table");
		return -1;
	}

	map_child_nodes(fs, fs->root, &inum);
	fs->root->inode_num = inum;
	fs->inode_table[inum] = fs->root;
	return 0;
}
