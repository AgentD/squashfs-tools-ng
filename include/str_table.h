/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * str_table.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef STR_TABLE_H
#define STR_TABLE_H

typedef struct str_bucket_t {
	struct str_bucket_t *next;
	char *str;
	size_t index;
	size_t refcount;
} str_bucket_t;

/* Stores strings in a hash table and assigns an incremental, unique ID to
   each string. Subsequent additions return the existing ID. The ID can be
   used for (hopefully) constant time lookup of the original string. */
typedef struct {
	str_bucket_t **buckets;
	size_t num_buckets;

	char **strings;
	size_t num_strings;
	size_t max_strings;
} str_table_t;

/* `size` is the number of hash table buckets to use internally.
   Returns 0 on success. Internally writes errors to stderr. */
int str_table_init(str_table_t *table, size_t size);

void str_table_cleanup(str_table_t *table);

/* Resolve a string to an incremental, unique ID
   Returns 0 on success. Internally writes errors to stderr. */
int str_table_get_index(str_table_t *table, const char *str, size_t *idx);

/* Resolve a unique ID to the string it represents.
   Returns NULL if the ID is unknown, i.e. out of bounds. */
const char *str_table_get_string(str_table_t *table, size_t index);

void str_table_reset_ref_count(str_table_t *table);

void str_table_add_ref(str_table_t *table, size_t index);

size_t str_table_get_ref_count(str_table_t *table, size_t index);

#endif /* STR_TABLE_H */
