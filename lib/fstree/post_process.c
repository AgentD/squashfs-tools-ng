/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * post_process.c
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

static void sort_recursive(tree_node_t *n)
{
	n->data.dir.children = tree_node_list_sort(n->data.dir.children);

	for (n = n->data.dir.children; n != NULL; n = n->next) {
		if (S_ISDIR(n->mode))
			sort_recursive(n);
	}
}

static file_info_t *file_list_dfs(tree_node_t *n)
{
	if (S_ISREG(n->mode)) {
		n->data.file.next = NULL;
		return &n->data.file;
	}

	if (S_ISDIR(n->mode)) {
		file_info_t *list = NULL, *last = NULL;

		for (n = n->data.dir.children; n != NULL; n = n->next) {
			if (list == NULL) {
				list = file_list_dfs(n);
				if (list == NULL)
					continue;
				last = list;
			} else {
				last->next = file_list_dfs(n);
			}

			while (last->next != NULL)
				last = last->next;
		}

		return list;
	}

	return NULL;
}

void fstree_post_process(fstree_t *fs)
{
	size_t inum = 1;

	sort_recursive(fs->root);

	fs->unique_inode_count = 0;
	map_child_nodes(fs, fs->root, &inum);
	fs->root->inode_num = inum;
	fs->unique_inode_count += 1;

	fs->files = file_list_dfs(fs->root);
}
