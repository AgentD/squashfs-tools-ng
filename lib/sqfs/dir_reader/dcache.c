/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dcache.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static int dcache_key_compare(const void *ctx, const void *l, const void *r)
{
	sqfs_u32 lhs = *((const sqfs_u32 *)l), rhs = *((const sqfs_u32 *)r);
	(void)ctx;

	return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

int sqfs_dir_reader_dcache_init(sqfs_dir_reader_t *rd, sqfs_u32 flags)
{
	if (!(flags & SQFS_DIR_READER_DOT_ENTRIES))
		return 0;

	return rbtree_init(&rd->dcache, sizeof(sqfs_u32), sizeof(sqfs_u64),
			   dcache_key_compare);
}

int sqfs_dir_reader_dcache_init_copy(sqfs_dir_reader_t *copy,
				     const sqfs_dir_reader_t *rd)
{
	if (!(rd->flags & SQFS_DIR_READER_DOT_ENTRIES))
		return 0;

	return rbtree_copy(&rd->dcache, &copy->dcache);
}

void sqfs_dir_reader_dcache_cleanup(sqfs_dir_reader_t *rd)
{
	if (!(rd->flags & SQFS_DIR_READER_DOT_ENTRIES))
		return;

	rbtree_cleanup(&rd->dcache);
}

int sqfs_dir_reader_dcache_add(sqfs_dir_reader_t *rd,
			       sqfs_u32 inode, sqfs_u64 ref)
{
	rbtree_node_t *node;

	if (!(rd->flags & SQFS_DIR_READER_DOT_ENTRIES))
		return 0;

	node = rbtree_lookup(&rd->dcache, &inode);
	if (node != NULL)
		return 0;

	return rbtree_insert(&rd->dcache, &inode, &ref);
}

int sqfs_dir_reader_dcache_find(sqfs_dir_reader_t *rd,
				sqfs_u32 inode, sqfs_u64 *ref)
{
	rbtree_node_t *node;

	if (!(rd->flags & SQFS_DIR_READER_DOT_ENTRIES))
		return SQFS_ERROR_NO_ENTRY;

	node = rbtree_lookup(&rd->dcache, &inode);
	if (node == NULL)
		return SQFS_ERROR_NO_ENTRY;

	*ref = *((sqfs_u64 *)rbtree_node_value(node));
	return 0;
}
