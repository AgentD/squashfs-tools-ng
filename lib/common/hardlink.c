/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hardlink.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"
#include "rbtree.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

static int map_nodes(rbtree_t *inumtree, sqfs_hard_link_t **out,
		     const sqfs_tree_node_t *n)
{
	const sqfs_tree_node_t *target;
	sqfs_hard_link_t *lnk;
	rbtree_node_t *tn;
	sqfs_u32 idx;
	int ret;

	/* XXX: refuse to generate hard links to directories */
	if (n->children != NULL) {
		for (n = n->children; n != NULL; n = n->next) {
			if (map_nodes(inumtree, out, n))
				return -1;
		}
		return 0;
	}

	if (!is_filename_sane((const char *)n->name, false))
		return 0;

	idx = n->inode->base.inode_number;
	tn = rbtree_lookup(inumtree, &idx);

	if (tn == NULL) {
		ret = rbtree_insert(inumtree, &idx, &n);
		if (ret != 0)
			goto fail_insert;
		return 0;
	}

	target = *((const sqfs_tree_node_t **)rbtree_node_value(tn));

	lnk = calloc(1, sizeof(*lnk));
	if (lnk == NULL)
		goto fail_oom;

	lnk->inode_number = idx;
	lnk->target = sqfs_tree_node_get_path(target);
	if (lnk->target == NULL)
		goto fail_oom;

	if (canonicalize_name(lnk->target) == 0) {
		lnk->next = (*out);
		(*out) = lnk;
	} else {
		free(lnk->target);
		free(lnk);
	}
	return 0;
fail_oom:
	fputs("detecting hard links in file system tree: out of memory\n",
	      stderr);
	free(lnk);
	return -1;
fail_insert:
	sqfs_perror(NULL, "detecting hard links in file system tree", ret);
	return -1;
}

static int compare_inum(const void *ctx, const void *lhs, const void *rhs)
{
	sqfs_u32 l = *((const sqfs_u32 *)lhs), r = *((const sqfs_u32 *)rhs);
	(void)ctx;

	return l < r ? -1 : (l > r ? 1 : 0);
}

int sqfs_tree_find_hard_links(const sqfs_tree_node_t *root,
			      sqfs_hard_link_t **out)
{
	sqfs_hard_link_t *lnk = NULL;
	rbtree_t inumtree;
	int ret;

	ret = rbtree_init(&inumtree, sizeof(sqfs_u32),
			  sizeof(sqfs_tree_node_t *),
			  compare_inum);
	if (ret != 0) {
		sqfs_perror(NULL,
			    "detecting hard links in file system tree", ret);
		return -1;
	}

	ret = map_nodes(&inumtree, out, root);
	rbtree_cleanup(&inumtree);

	if (ret != 0) {
		while ((*out) != NULL) {
			lnk = (*out);
			(*out) = lnk->next;
			free(lnk->target);
			free(lnk);
		}
		return -1;
	}

	return 0;
}
