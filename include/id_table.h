/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ID_TABLE_H
#define ID_TABLE_H

#include <stdint.h>
#include <stddef.h>

#include "compress.h"

/**
 * @struct id_table_t
 *
 * @brief Encapsulates the ID table used by SquashFS
 */
typedef struct {
	/**
	 * @brief Array of unique 32 bit IDs
	 */
	uint32_t *ids;

	/**
	 * @brief Number of 32 bit IDs stored in the array
	 */
	size_t num_ids;

	/**
	 * @brief Actual size of the array, i.e. maximum available
	 */
	size_t max_ids;
} id_table_t;

/**
 * @brief Initialize an ID table
 *
 * @memberof id_table_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * @param tbl A pointer to an uninitialized ID table
 *
 * @return Zero on success, -1 on failure
 */
int id_table_init(id_table_t *tbl);

/**
 * @brief Cleanup and free an ID table
 *
 * @memberof id_table_t
 *
 * @param tbl A pointer to an ID table
 */
void id_table_cleanup(id_table_t *tbl);

/**
 * @brief Resolve a 32 bit to a 16 bit table index
 *
 * @memberof id_table_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * @param tbl A pointer to an ID table
 * @param id  A 32 bit ID to resolve
 * @param out Returns the 16 bit table index
 *
 * @return Zero on success, -1 on failure
 */
int id_table_id_to_index(id_table_t *tbl, uint32_t id, uint16_t *out);

/**
 * @brief Write an ID table to a SquashFS image
 *
 * @memberof id_table_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * @param tbl   A pointer to an ID table
 * @param outfd A file descriptor to write the data to
 * @param super A pointer to the SquashFS super block to store the ID table
 *              offset in
 * @param cmp   A pointer to the compressor to use for compressing the ID
 *              table meta data blocks
 *
 * @return Zero on success, -1 on failure
 */
int id_table_write(id_table_t *tbl, int outfd, sqfs_super_t *super,
		   compressor_t *cmp);

/**
 * @brief Read an ID table from a SquashFS image
 *
 * @memberof id_table_t
 *
 * @note This function internally prints error message to stderr on failure
 *
 * @param tbl   A pointer to an ID table
 * @param fd    A file descriptor to read the data from
 * @param super A pointer to the SquashFS super block that tells us where
 *              the ID table is stored
 * @param cmp   A pointer to the compressor to use for extracting the ID
 *              table meta data blocks
 *
 * @return Zero on success, -1 on failure
 */
int id_table_read(id_table_t *tbl, int fd, sqfs_super_t *super,
		   compressor_t *cmp);

#endif /* ID_TABLE_H */
