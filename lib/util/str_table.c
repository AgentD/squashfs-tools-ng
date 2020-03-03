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

static int strings_grow(str_table_t *table)
{
	size_t newsz;
	void *new;

	if (table->num_strings < table->max_strings)
		return 0;

	newsz = table->max_strings ? (table->max_strings * 2) : 16;
	new = realloc(table->strings, sizeof(table->strings[0]) * newsz);

	if (new == NULL)
		return SQFS_ERROR_ALLOC;

	table->strings = new;
	table->max_strings = newsz;
	return 0;
}

int str_table_init(str_table_t *table, size_t size)
{
	memset(table, 0, sizeof(*table));

	table->buckets = alloc_array(size, sizeof(table->buckets[0]));
	table->num_buckets = size;

	if (table->buckets == NULL)
		return SQFS_ERROR_ALLOC;

	return 0;
}

int str_table_copy(str_table_t *dst, const str_table_t *src)
{
	str_bucket_t *bucket, *it, **next;
	size_t i;

	dst->num_strings = src->num_strings;
	dst->max_strings = src->max_strings;
	dst->strings = calloc(sizeof(dst->strings[0]), src->num_strings);

	if (dst->strings == NULL)
		return -1;

	for (i = 0; i < src->num_strings; ++i) {
		dst->strings[i] = strdup(src->strings[i]);

		if (dst->strings[i] == NULL)
			goto fail_str;
	}

	dst->num_buckets = src->num_buckets;
	dst->buckets = calloc(sizeof(dst->buckets[0]), src->num_buckets);

	if (dst->buckets == NULL)
		goto fail_str;

	for (i = 0; i < src->num_buckets; ++i) {
		next = &(dst->buckets[i]);

		for (it = src->buckets[i]; it != NULL; it = it->next) {
			bucket = malloc(sizeof(*bucket));
			if (bucket == NULL)
				goto fail_buck;

			bucket->next = NULL;
			bucket->index = it->index;
			bucket->refcount = it->refcount;
			bucket->str = dst->strings[bucket->index];

			*next = bucket;
			next = &(bucket->next);
		}
	}

	return 0;
fail_buck:
	for (i = 0; i < dst->num_buckets; ++i) {
		while (dst->buckets[i] != NULL) {
			bucket = dst->buckets[i];
			dst->buckets[i] = bucket->next;

			free(bucket);
		}
	}
	free(dst->buckets);
fail_str:
	for (i = 0; i < dst->num_strings; ++i)
		free(dst->strings[i]);

	free(dst->strings);
	dst->strings = NULL;
	return -1;
}

void str_table_cleanup(str_table_t *table)
{
	str_bucket_t *bucket;
	size_t i;

	for (i = 0; i < table->num_buckets; ++i) {
		while (table->buckets[i] != NULL) {
			bucket = table->buckets[i];
			table->buckets[i] = bucket->next;

			free(bucket->str);
			free(bucket);
		}
	}

	free(table->buckets);
	free(table->strings);
	memset(table, 0, sizeof(*table));
}

int str_table_get_index(str_table_t *table, const char *str, size_t *idx)
{
	str_bucket_t *bucket;
	sqfs_u32 hash;
	size_t index;
	int err;

	hash = strhash(str);
	index = hash % table->num_buckets;
	bucket = table->buckets[index];

	while (bucket != NULL) {
		if (strcmp(bucket->str, str) == 0) {
			*idx = bucket->index;
			return 0;
		}

		bucket = bucket->next;
	}

	err = strings_grow(table);
	if (err)
		return err;

	bucket = calloc(1, sizeof(*bucket));
	if (bucket == NULL)
		goto fail_oom;

	bucket->str = strdup(str);
	if (bucket->str == NULL)
		goto fail_oom;

	bucket->index = table->num_strings;
	table->strings[table->num_strings++] = bucket->str;
	*idx = bucket->index;

	bucket->next = table->buckets[index];
	table->buckets[index] = bucket;
	return 0;
fail_oom:
	free(bucket);
	return SQFS_ERROR_ALLOC;
}

const char *str_table_get_string(str_table_t *table, size_t index)
{
	if (index >= table->num_strings)
		return NULL;

	return table->strings[index];
}

static str_bucket_t *bucket_by_index(str_table_t *table, size_t index)
{
	str_bucket_t *bucket = NULL;
	sqfs_u32 hash;

	if (index < table->num_strings) {
		hash = strhash(table->strings[index]);
		bucket = table->buckets[hash % table->num_buckets];

		while (bucket != NULL && bucket->index != index)
			bucket = bucket->next;
	}

	return bucket;
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

size_t str_table_get_ref_count(str_table_t *table, size_t index)
{
	str_bucket_t *bucket = bucket_by_index(table, index);

	return bucket != NULL ? bucket->refcount : 0;
}
