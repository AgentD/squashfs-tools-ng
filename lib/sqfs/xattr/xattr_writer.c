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
	kv_block_desc_t *blk, *it, **next;
	sqfs_xattr_writer_t *copy;

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

	next = &(copy->kv_blocks);

	for (it = xwr->kv_blocks; it != NULL; it = it->next) {
		blk = malloc(sizeof(*blk));
		if (blk == NULL)
			goto fail_blk;

		memcpy(blk, it, sizeof(*it));
		blk->next = NULL;

		*next = blk;
		next = &(blk->next);
	}

	return (sqfs_object_t *)copy;
fail_blk:
	while (copy->kv_blocks != NULL) {
		blk = copy->kv_blocks;
		copy->kv_blocks = copy->kv_blocks->next;
		free(blk);
	}
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
	kv_block_desc_t *blk;

	while (xwr->kv_blocks != NULL) {
		blk = xwr->kv_blocks;
		xwr->kv_blocks = xwr->kv_blocks->next;
		free(blk);
	}

	free(xwr->kv_pairs);
	str_table_cleanup(&xwr->values);
	str_table_cleanup(&xwr->keys);
	free(xwr);
}

sqfs_xattr_writer_t *sqfs_xattr_writer_create(void)
{
	sqfs_xattr_writer_t *xwr = calloc(1, sizeof(*xwr));

	if (str_table_init(&xwr->keys, XATTR_KEY_BUCKETS))
		goto fail_keys;

	if (str_table_init(&xwr->values, XATTR_VALUE_BUCKETS))
		goto fail_values;

	xwr->max_pairs = XATTR_INITIAL_PAIR_CAP;
	xwr->kv_pairs = alloc_array(sizeof(xwr->kv_pairs[0]), xwr->max_pairs);

	if (xwr->kv_pairs == NULL)
		goto fail_pairs;

	((sqfs_object_t *)xwr)->copy = xattr_writer_copy;
	((sqfs_object_t *)xwr)->destroy = xattr_writer_destroy;
	return xwr;
fail_pairs:
	str_table_cleanup(&xwr->values);
fail_values:
	str_table_cleanup(&xwr->keys);
fail_keys:
	free(xwr);
	return NULL;
}
