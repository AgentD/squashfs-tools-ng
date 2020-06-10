/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer_record.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xattr_writer.h"

static const char *hexmap = "0123456789ABCDEF";

static char *to_base32(const void *input, size_t size)
{
	const sqfs_u8 *in = input;
	char *out, *ptr;
	size_t i;

	out = malloc(2 * size + 1);
	if (out == NULL)
		return NULL;

	ptr = out;

	for (i = 0; i < size; ++i) {
		*(ptr++) = hexmap[ in[i]       & 0x0F];
		*(ptr++) = hexmap[(in[i] >> 4) & 0x0F];
	}

	*ptr = '\0';
	return out;
}

static int compare_u64(const void *a, const void *b)
{
	sqfs_u64 lhs = *((const sqfs_u64 *)a);
	sqfs_u64 rhs = *((const sqfs_u64 *)b);

	return (lhs < rhs ? -1 : (lhs > rhs ? 1 : 0));
}

int sqfs_xattr_writer_begin(sqfs_xattr_writer_t *xwr, sqfs_u32 flags)
{
	if (flags != 0)
		return SQFS_ERROR_UNSUPPORTED;

	xwr->kv_start = xwr->num_pairs;
	return 0;
}

int sqfs_xattr_writer_add(sqfs_xattr_writer_t *xwr, const char *key,
			  const void *value, size_t size)
{
	size_t i, key_index, old_value_index, value_index, new_count;
	sqfs_u64 kv_pair, *new;
	char *value_str;
	int err;

	if (sqfs_get_xattr_prefix_id(key) < 0)
		return SQFS_ERROR_UNSUPPORTED;

	/* resolve key and value into unique, incremental IDs */
	err = str_table_get_index(&xwr->keys, key, &key_index);
	if (err)
		return err;

	value_str = to_base32(value, size);
	if (value_str == NULL)
		return SQFS_ERROR_ALLOC;

	err = str_table_get_index(&xwr->values, value_str, &value_index);
	free(value_str);
	if (err)
		return err;

	str_table_add_ref(&xwr->values, value_index);

	if (sizeof(size_t) > sizeof(sqfs_u32)) {
		if (key_index > 0x0FFFFFFFFUL || value_index > 0x0FFFFFFFFUL)
			return SQFS_ERROR_OVERFLOW;
	}

	/* bail if already have the pair, overwrite if we have the key */
	kv_pair = MK_PAIR(key_index, value_index);

	for (i = xwr->kv_start; i < xwr->num_pairs; ++i) {
		if (xwr->kv_pairs[i] == kv_pair)
			return 0;

		if (GET_KEY(xwr->kv_pairs[i]) == key_index) {
			old_value_index = GET_VALUE(xwr->kv_pairs[i]);

			str_table_del_ref(&xwr->values, old_value_index);

			xwr->kv_pairs[i] = kv_pair;
			return 0;
		}
	}

	/* append it to the list */
	if (xwr->max_pairs == xwr->num_pairs) {
		new_count = xwr->max_pairs * 2;
		new = realloc(xwr->kv_pairs,
			      sizeof(xwr->kv_pairs[0]) * new_count);

		if (new == NULL)
			return SQFS_ERROR_ALLOC;

		xwr->kv_pairs = new;
		xwr->max_pairs = new_count;
	}

	xwr->kv_pairs[xwr->num_pairs++] = kv_pair;
	return 0;
}

int sqfs_xattr_writer_end(sqfs_xattr_writer_t *xwr, sqfs_u32 *out)
{
	kv_block_desc_t *blk, *blk_prev;
	sqfs_u32 index;
	size_t count;
	int ret;

	count = xwr->num_pairs - xwr->kv_start;
	if (count == 0) {
		*out = 0xFFFFFFFF;
		return 0;
	}

	qsort(xwr->kv_pairs + xwr->kv_start, count,
	      sizeof(xwr->kv_pairs[0]), compare_u64);

	blk_prev = NULL;
	blk = xwr->kv_blocks;
	index = 0;

	while (blk != NULL) {
		if (blk->count == count) {
			ret = memcmp(xwr->kv_pairs + blk->start,
				     xwr->kv_pairs + xwr->kv_start,
				     sizeof(xwr->kv_pairs[0]) * count);

			if (ret == 0)
				break;
		}

		if (index == 0xFFFFFFFF)
			return SQFS_ERROR_OVERFLOW;

		++index;
		blk_prev = blk;
		blk = blk->next;
	}

	if (blk != NULL) {
		xwr->num_pairs = xwr->kv_start;
	} else {
		blk = calloc(1, sizeof(*blk));
		if (blk == NULL)
			return SQFS_ERROR_ALLOC;

		blk->start = xwr->kv_start;
		blk->count = count;

		if (blk_prev == NULL) {
			xwr->kv_blocks = blk;
		} else {
			blk_prev->next = blk;
		}

		xwr->num_blocks += 1;
	}

	*out = index;
	return 0;
}
