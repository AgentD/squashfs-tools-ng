/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * post_process.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>

static void swap_link_with_target(tree_node_t *node)
{
	tree_node_t *tgt, *it;

	tgt = node->data.target_node;

	node->xattr_idx = tgt->xattr_idx;
	node->uid = tgt->uid;
	node->gid = tgt->gid;
	node->inode_num = tgt->inode_num;
	node->mod_time = tgt->mod_time;
	node->mode = tgt->mode;
	node->link_count = tgt->link_count;
	node->inode_ref = tgt->inode_ref;

	/* FIXME: data pointers now point to foreign node! */
	node->data = tgt->data;

	tgt->mode = FSTREE_MODE_HARD_LINK_RESOLVED;
	tgt->data.target_node = node;

	if (S_ISDIR(node->mode)) {
		for (it = node->data.dir.children; it != NULL; it = it->next)
			it->parent = node;
	}
}

static void hard_link_snap(tree_node_t *n)
{
	/* XXX: the hard-link-vs-target swap may create hard links
	   pointing to hard links, making this necessary */
	while (n->data.target_node->mode == FSTREE_MODE_HARD_LINK_RESOLVED)
		n->data.target_node = n->data.target_node->data.target_node;
}

static void alloc_inode_num_dfs(fstree_t *fs, tree_node_t *root)
{
	bool has_subdirs = false;
	tree_node_t *it, *tgt;

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		if (it->mode == FSTREE_MODE_HARD_LINK_RESOLVED) {
			hard_link_snap(it);
			tgt = it->data.target_node;

			if (tgt->inode_num == 0 && tgt->parent != root)
				swap_link_with_target(it);
		}

		if (S_ISDIR(it->mode))
			has_subdirs = true;
	}

	if (has_subdirs) {
		for (it = root->data.dir.children; it != NULL; it = it->next) {
			if (S_ISDIR(it->mode))
				alloc_inode_num_dfs(fs, it);
		}
	}

	for (it = root->data.dir.children; it != NULL; it = it->next) {
		if (it->mode == FSTREE_MODE_HARD_LINK_RESOLVED) {
			hard_link_snap(it);
		} else {
			it->inode_num = fs->unique_inode_count + 1;
			fs->unique_inode_count += 1;
		}
	}
}

static int resolve_hard_links_dfs(fstree_t *fs, tree_node_t *n)
{
	tree_node_t *it;

	if (n->mode == FSTREE_MODE_HARD_LINK) {
		if (fstree_resolve_hard_link(fs, n))
			goto fail_link;

		assert(n->mode == FSTREE_MODE_HARD_LINK_RESOLVED);
		it = n->data.target_node;

		if (S_ISDIR(it->mode) && it->data.dir.visited)
			goto fail_link_loop;
	} else if (S_ISDIR(n->mode)) {
		n->data.dir.visited = true;

		for (it = n->data.dir.children; it != NULL; it = it->next) {
			if (resolve_hard_links_dfs(fs, it))
				return -1;
		}

		n->data.dir.visited = false;
	}

	return 0;
fail_link: {
	char *path = fstree_get_path(n);
	fprintf(stderr, "Resolving hard link '%s' -> '%s': %s\n",
		path == NULL ? n->name : path, n->data.target,
		strerror(errno));
	free(path);
}
	return -1;
fail_link_loop: {
	char *npath = fstree_get_path(n);
	char *tpath = fstree_get_path(it);
	fprintf(stderr, "Hard link loop detected in '%s' -> '%s'\n",
		npath == NULL ? n->name : npath,
		tpath == NULL ? it->name : tpath);
	free(npath);
	free(tpath);
}
	return -1;
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

static void map_inodes_dfs(fstree_t *fs, tree_node_t *n)
{
	if (n->mode == FSTREE_MODE_HARD_LINK_RESOLVED)
		return;

	fs->inodes[n->inode_num - 1] = n;

	if (S_ISDIR(n->mode)) {
		for (n = n->data.dir.children; n != NULL; n = n->next)
			map_inodes_dfs(fs, n);
	}
}

int fstree_post_process(fstree_t *fs)
{
	sort_recursive(fs->root);

	if (resolve_hard_links_dfs(fs, fs->root))
		return -1;

	fs->unique_inode_count = 0;
	alloc_inode_num_dfs(fs, fs->root);
	fs->root->inode_num = fs->unique_inode_count + 1;
	fs->unique_inode_count += 1;

	fs->inodes = calloc(sizeof(fs->inodes[0]), fs->unique_inode_count);
	if (fs->inodes == NULL) {
		perror("Allocating inode list");
		return -1;
	}

	map_inodes_dfs(fs, fs->root);

	fs->files = file_list_dfs(fs->root);
	return 0;
}
