/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * post_process.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

static int alloc_inode_num_dfs(fstree_t *fs, tree_node_t *root)
{
	bool has_subdirs = false;
	tree_node_t *it;
	size_t inum;

	for (it = root->data.children; it != NULL; it = it->next) {
		if (S_ISDIR(it->mode)) {
			has_subdirs = true;
			break;
		}
	}

	if (has_subdirs) {
		for (it = root->data.children; it != NULL; it = it->next) {
			if (S_ISDIR(it->mode)) {
				if (alloc_inode_num_dfs(fs, it))
					return -1;
			}
		}
	}

	for (it = root->data.children; it != NULL; it = it->next) {
		if (it->mode != FSTREE_MODE_HARD_LINK_RESOLVED) {
			if (SZ_ADD_OV(fs->unique_inode_count, 1, &inum))
				goto fail_ov;

			if ((sizeof(size_t) > sizeof(sqfs_u32)) &&
			    inum > 0x0FFFFFFFFUL) {
				goto fail_ov;
			}

			it->inode_num = (sqfs_u32)inum;
			fs->unique_inode_count = inum;
		}
	}

	return 0;
fail_ov:
	fputs("Too many inodes.\n", stderr);
	return -1;
}

static tree_node_t *file_list_dfs(tree_node_t *n)
{
	if (S_ISREG(n->mode)) {
		n->next_by_type = NULL;
		return n;
	}

	if (S_ISDIR(n->mode)) {
		tree_node_t *list = NULL, *last = NULL;

		for (n = n->data.children; n != NULL; n = n->next) {
			if (list == NULL) {
				list = file_list_dfs(n);
				if (list == NULL)
					continue;
				last = list;
			} else {
				last->next_by_type = file_list_dfs(n);
			}

			while (last->next_by_type != NULL)
				last = last->next_by_type;
		}

		return list;
	}

	return NULL;
}

static void map_inodes_dfs(fstree_t *fs, tree_node_t *n)
{
	if (n->mode == FSTREE_MODE_HARD_LINK_RESOLVED)
		return;

	fs->inodes[n->inode_num - 1] = n;

	if (S_ISDIR(n->mode)) {
		for (n = n->data.children; n != NULL; n = n->next)
			map_inodes_dfs(fs, n);
	}
}

static void reorder_hard_links(fstree_t *fs)
{
	size_t i, j, tgt_idx;
	tree_node_t *it, *tgt;

	for (i = 0; i < fs->unique_inode_count; ++i) {
		if (!S_ISDIR(fs->inodes[i]->mode))
			continue;

		it = fs->inodes[i]->data.children;

		for (; it != NULL; it = it->next) {
			if (it->mode != FSTREE_MODE_HARD_LINK_RESOLVED)
				continue;

			tgt = it->data.target_node;
			tgt_idx = tgt->inode_num - 1;

			if (tgt_idx <= i)
				continue;

			for (j = tgt_idx; j > i; --j) {
				fs->inodes[j] = fs->inodes[j - 1];
				fs->inodes[j]->inode_num += 1;
			}

			fs->inodes[i] = tgt;

			/* XXX: the possible overflow is checked for
			   during allocation */
			tgt->inode_num = (sqfs_u32)(i + 1);
			++i;
		}
	}
}

int fstree_post_process(fstree_t *fs)
{
	size_t inum;

	if (fstree_resolve_hard_links(fs))
		return -1;

	fs->unique_inode_count = 0;

	if (alloc_inode_num_dfs(fs, fs->root))
		return -1;

	if (SZ_ADD_OV(fs->unique_inode_count, 1, &inum))
		goto fail_root_ov;

	if ((sizeof(size_t) > sizeof(sqfs_u32)) && inum > 0x0FFFFFFFFUL)
		goto fail_root_ov;

	fs->root->inode_num = (sqfs_u32)inum;
	fs->unique_inode_count = inum;

	fs->inodes = calloc(sizeof(fs->inodes[0]), fs->unique_inode_count);
	if (fs->inodes == NULL) {
		perror("Allocating inode list");
		return -1;
	}

	map_inodes_dfs(fs, fs->root);
	reorder_hard_links(fs);

	fs->files = file_list_dfs(fs->root);
	return 0;
fail_root_ov:
	fputs("Too many inodes, cannot allocate number for root.\n", stderr);
	return -1;
}
