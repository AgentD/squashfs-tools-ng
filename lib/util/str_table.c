/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * str_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sqfs/error.h"
#include "str_table.h"
#include "util.h"

/* R5 hash function (borrowed from reiserfs) */
static sqfs_u32 strhash(const char *s)
{
	const signed char *str = (const signed char *)s;
	sqfs_u32 a = 0;

	while (*str != '\0') {
		a += *str << 4;
		a += *str >> 4;
		a *= 11;
		str++;
	}

	return a;
}

static bool key_equals_function(void *user, const void *a, const void *b)
{
	(void)user;
	return strcmp(a, b) == 0;
}

int str_table_init(str_table_t *table)
{
	memset(table, 0, sizeof(*table));

	if (array_init(&table->bucket_ptrs, sizeof(str_bucket_t *), 0))
		goto fail_arr;

	table->ht = hash_table_create(NULL, key_equals_function);
	if (table->ht == NULL)
		goto fail_ht;

	return 0;
fail_ht:
	array_cleanup(&table->bucket_ptrs);
fail_arr:
	memset(table, 0, sizeof(*table));
	return SQFS_ERROR_ALLOC;
}

int str_table_copy(str_table_t *dst, const str_table_t *src)
{
	str_bucket_t *bucket, **array;
	int ret;

	ret = array_init_copy(&dst->bucket_ptrs, &src->bucket_ptrs);
	if (ret != 0)
		return ret;

	dst->ht = hash_table_clone(src->ht);
	if (dst->ht == NULL) {
		array_cleanup(&dst->bucket_ptrs);
		return SQFS_ERROR_ALLOC;
	}

	array = (str_bucket_t **)dst->bucket_ptrs.data;

	hash_table_foreach(dst->ht, ent) {
		bucket = alloc_flex(sizeof(*bucket), 1, strlen(ent->key) + 1);
		if (bucket == NULL) {
			str_table_cleanup(dst);
			return SQFS_ERROR_ALLOC;
		}

		memcpy(bucket, ent->data,
		       sizeof(*bucket) + strlen(ent->key) + 1);

		ent->data = bucket;
		ent->key = bucket->string;

		array[bucket->index] = bucket;
	}

	return 0;
}

void str_table_cleanup(str_table_t *table)
{
	hash_table_foreach(table->ht, ent) {
		free(ent->data);
		ent->data = NULL;
		ent->key = NULL;
	}

	hash_table_destroy(table->ht, NULL);
	array_cleanup(&table->bucket_ptrs);
	memset(table, 0, sizeof(*table));
}

int str_table_get_index(str_table_t *table, const char *str, size_t *idx)
{
	struct hash_entry *ent;
	str_bucket_t *new;
	sqfs_u32 hash;

	hash = strhash(str);
	ent = hash_table_search_pre_hashed(table->ht, hash, str);

	if (ent != NULL) {
		*idx = ((str_bucket_t *)ent->data)->index;
		return 0;
	}

	new = alloc_flex(sizeof(*new), 1, strlen(str) + 1);
	if (new == NULL)
		return SQFS_ERROR_ALLOC;

	new->index = table->next_index;
	strcpy(new->string, str);

	ent = hash_table_insert_pre_hashed(table->ht, hash, str, new);
	if (ent == NULL) {
		free(new);
		return SQFS_ERROR_ALLOC;
	}

	ent->key = new->string;

	if (array_append(&table->bucket_ptrs, &new) != 0) {
		free(new);
		ent->key = NULL;
		ent->data = NULL;
		return SQFS_ERROR_ALLOC;
	}

	*idx = table->next_index++;
	return 0;
}

static str_bucket_t *bucket_by_index(const str_table_t *table, size_t index)
{
	if (index >= table->bucket_ptrs.used)
		return NULL;

	return ((str_bucket_t **)table->bucket_ptrs.data)[index];
}

const char *str_table_get_string(const str_table_t *table, size_t index)
{
	str_bucket_t *bucket = bucket_by_index(table, index);

	return bucket == NULL ? NULL : bucket->string;
}

void str_table_add_ref(str_table_t *table, size_t index)
{
	str_bucket_t *bucket = bucket_by_index(table, index);

	if (bucket != NULL && bucket->refcount < ~((size_t)0))
		bucket->refcount += 1;
}

void str_table_del_ref(str_table_t *table, size_t index)
{
	str_bucket_t *bucket = bucket_by_index(table, index);

	if (bucket != NULL && bucket->refcount > 0)
		bucket->refcount -= 1;
}

size_t str_table_get_ref_count(const str_table_t *table, size_t index)
{
	str_bucket_t *bucket = bucket_by_index(table, index);

	return bucket != NULL ? bucket->refcount : 0;
}
