/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef STR_TABLE_H
#define STR_TABLE_H

typedef struct str_bucket_t {
	struct str_bucket_t *next;
	char *str;
	size_t index;
} str_bucket_t;

/**
 * @struct str_table_t
 *
 * @brief A data structure that manages incremental, unique IDs for strings
 *
 * A string table allocates IDs for strings, and provides fast lookup for ID by
 * string and string by ID.
 */
typedef struct {
	str_bucket_t **buckets;
	size_t num_buckets;

	char **strings;
	size_t num_strings;
	size_t max_strings;
} str_table_t;

/**
 * @brief Initialize a string table
 *
 * @memberof str_table_t
 *
 * @param size The number of hash table buckets to use internally
 *
 * @return Zero on success, -1 on failure (reports error to stderr)
 */
int str_table_init(str_table_t *table, size_t size);

/**
 * @brief Free all memory used by a string table
 *
 * @memberof str_table_t
 *
 * @param table A pointer to a string table object
 */
void str_table_cleanup(str_table_t *table);

/**
 * @brief Resolve a string to an incremental, unique ID
 *
 * @memberof str_table_t
 *
 * If the string is passed to this function for the first time, a new ID
 * is allocated for the string.
 *
 * @param table A pointer to a string table object
 * @param str   A pointer to a string to resolve
 * @param idx   Returns the unique ID of the string
 *
 * @return Zero on success, -1 on failure (reports error to stderr)
 */
int str_table_get_index(str_table_t *table, const char *str, size_t *idx);

/**
 * @brief Resolve a unique ID to the string it represents
 *
 * @memberof str_table_t
 *
 * @param table A pointer to a string table object
 * @param index The ID to resolve
 *
 * @return A pointer to the string or NULL if it hasn't been added yet
 */
const char *str_table_get_string(str_table_t *table, size_t index);

#endif /* STR_TABLE_H */
