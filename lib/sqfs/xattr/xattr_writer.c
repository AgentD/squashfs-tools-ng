/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xattr_writer.h"

static sqfs_object_t *xattr_writer_copy(const sqfs_object_t *obj)
{
	const sqfs_xattr_writer_t *xwr = (const sqfs_xattr_writer_t *)obj;
	sqfs_xattr_writer_t *copy;
	kv_block_desc_t *it;

	copy = calloc(1, sizeof(*copy));
	if (copy == NULL)
		return NULL;

	memcpy(copy, xwr, sizeof(*xwr));

	if (str_table_copy(&copy->keys, &xwr->keys))
		goto fail_keys;

	if (str_table_copy(&copy->values, &xwr->values))
		goto fail_values;

	copy->max_pairs = xwr->num_pairs;
	copy->num_pairs = xwr->num_pairs;

	copy->kv_pairs = malloc(sizeof(copy->kv_pairs[0]) * xwr->num_pairs);
	if (copy->kv_pairs == NULL)
		goto fail_pairs;

	memcpy(copy->kv_pairs, xwr->kv_pairs,
	       sizeof(copy->kv_pairs[0]) * xwr->num_pairs);

	if (rbtree_copy(&xwr->kv_block_tree, &copy->kv_block_tree) != 0)
		goto fail_tree;

	for (it = xwr->kv_block_first; it != NULL; it = it->next) {
		rbtree_node_t *n = rbtree_lookup(&copy->kv_block_tree, it);

		if (copy->kv_block_last == NULL) {
			copy->kv_block_first = rbtree_node_key(n);
			copy->kv_block_last = copy->kv_block_first;
		} else {
			copy->kv_block_last->next = rbtree_node_key(n);
			copy->kv_block_last = copy->kv_block_last->next;
		}

		copy->kv_block_last->next = NULL;
	}

	return (sqfs_object_t *)copy;
fail_tree:
	free(copy->kv_pairs);
fail_pairs:
	str_table_cleanup(&copy->values);
fail_values:
	str_table_cleanup(&copy->keys);
fail_keys:
	free(copy);
	return NULL;
}

static void xattr_writer_destroy(sqfs_object_t *obj)
{
	sqfs_xattr_writer_t *xwr = (sqfs_xattr_writer_t *)obj;

	rbtree_cleanup(&xwr->kv_block_tree);
	free(xwr->kv_pairs);
	str_table_cleanup(&xwr->values);
	str_table_cleanup(&xwr->keys);
	free(xwr);
}

static int block_compare(const void *context,
			 const void *lhs, const void *rhs)
{
	const sqfs_xattr_writer_t *xwr = context;
	const kv_block_desc_t *l = lhs, *r = rhs;

	if (l->count != r->count)
		return l->count < r->count ? -1 : 1;

	if (l->start == r->start)
		return 0;

	return memcmp(xwr->kv_pairs + l->start,
		      xwr->kv_pairs + r->start,
		      l->count * sizeof(xwr->kv_pairs[0]));
}

sqfs_xattr_writer_t *sqfs_xattr_writer_create(sqfs_u32 flags)
{
	sqfs_xattr_writer_t *xwr;

	if (flags != 0)
		return NULL;

	xwr = calloc(1, sizeof(*xwr));
	if (xwr == NULL)
		return NULL;

	if (str_table_init(&xwr->keys, XATTR_KEY_BUCKETS))
		goto fail_keys;

	if (str_table_init(&xwr->values, XATTR_VALUE_BUCKETS))
		goto fail_values;

	xwr->max_pairs = XATTR_INITIAL_PAIR_CAP;
	xwr->kv_pairs = alloc_array(sizeof(xwr->kv_pairs[0]), xwr->max_pairs);

	if (xwr->kv_pairs == NULL)
		goto fail_pairs;

	if (rbtree_init(&xwr->kv_block_tree, sizeof(kv_block_desc_t),
			sizeof(sqfs_u32), block_compare)) {
		goto fail_tree;
	}

	xwr->kv_block_tree.key_context = xwr;

	((sqfs_object_t *)xwr)->copy = xattr_writer_copy;
	((sqfs_object_t *)xwr)->destroy = xattr_writer_destroy;
	return xwr;
fail_tree:
	free(xwr->kv_pairs);
fail_pairs:
	str_table_cleanup(&xwr->values);
fail_values:
	str_table_cleanup(&xwr->keys);
fail_keys:
	free(xwr);
	return NULL;
}
