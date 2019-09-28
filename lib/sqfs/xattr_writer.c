/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/xattr_writer.h"
#include "sqfs/meta_writer.h"
#include "sqfs/super.h"
#include "sqfs/xattr.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"

#include "str_table.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>


#define XATTR_KEY_BUCKETS 31
#define XATTR_VALUE_BUCKETS 511
#define XATTR_INITIAL_PAIR_CAP 128

#define MK_PAIR(key, value) (((sqfs_u64)(key) << 32UL) | (sqfs_u64)(value))
#define GET_KEY(pair) ((pair >> 32UL) & 0x0FFFFFFFFUL)
#define GET_VALUE(pair) (pair & 0x0FFFFFFFFUL)


static const char *hexmap = "0123456789ABCDEF";

static char *to_base32(const void *input, size_t size)
{
	const uint8_t *in = input;
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

static void *from_base32(const char *input, size_t *size_out)
{
	uint8_t lo, hi, *out, *ptr;
	size_t len;

	len = strlen(input);
	*size_out = len / 2;

	out = malloc(*size_out);
	if (out == NULL)
		return NULL;

	ptr = out;

	while (*input != '\0') {
		lo = strchr(hexmap, *(input++)) - hexmap;
		hi = strchr(hexmap, *(input++)) - hexmap;

		*(ptr++) = lo | (hi << 4);
	}

	return out;
}

static int compare_u64(const void *a, const void *b)
{
	sqfs_u64 lhs = *((const sqfs_u64 *)a);
	sqfs_u64 rhs = *((const sqfs_u64 *)b);

	return (lhs < rhs ? -1 : (lhs > rhs ? 1 : 0));
}



typedef struct kv_block_desc_t {
	struct kv_block_desc_t *next;
	size_t start;
	size_t count;
} kv_block_desc_t;

struct sqfs_xattr_writer_t {
	str_table_t keys;
	str_table_t values;

	sqfs_u64 *kv_pairs;
	size_t max_pairs;
	size_t num_pairs;

	size_t kv_start;

	kv_block_desc_t *kv_blocks;
	size_t num_blocks;
};


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

	return xwr;
fail_pairs:
	str_table_cleanup(&xwr->values);
fail_values:
	str_table_cleanup(&xwr->keys);
fail_keys:
	free(xwr);
	return NULL;
}

void sqfs_xattr_writer_destroy(sqfs_xattr_writer_t *xwr)
{
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

int sqfs_xattr_writer_begin(sqfs_xattr_writer_t *xwr)
{
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

	if (!sqfs_has_xattr(key))
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
	size_t i, count, value_idx;
	sqfs_u32 index;
	int ret;

	count = xwr->num_pairs - xwr->kv_start;

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
		for (i = 0; i < count; ++i) {
			value_idx = GET_VALUE(xwr->kv_pairs[xwr->kv_start + i]);
			str_table_del_ref(&xwr->values, value_idx);

			value_idx = GET_VALUE(xwr->kv_pairs[blk->start + i]);
			str_table_add_ref(&xwr->values, value_idx);
		}

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

/*****************************************************************************/

int sqfs_xattr_writer_flush(sqfs_xattr_writer_t *xwr, sqfs_file_t *file,
			    sqfs_super_t *super, sqfs_compressor_t *cmp)
{
	(void)xwr;
	(void)file;
	(void)super;
	(void)cmp;
	return 0;
}
