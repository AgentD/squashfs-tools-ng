/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef ID_TABLE_H
#define ID_TABLE_H

#include "config.h"

#include <stdint.h>
#include <stddef.h>

#include "compress.h"

/* Encapsulates the ID table used by SquashFS */
typedef struct {
	/* Array of unique 32 bit IDs */
	uint32_t *ids;

	/* Number of 32 bit IDs stored in the array */
	size_t num_ids;

	/* Actual size of the array, i.e. maximum available */
	size_t max_ids;
} id_table_t;

/* Returns 0 on success. Prints error message to stderr on failure. */
int id_table_init(id_table_t *tbl);

void id_table_cleanup(id_table_t *tbl);

/* Resolve a 32 bit to a 16 bit table index.
   Returns 0 on success. Internally prints errors to stderr. */
int id_table_id_to_index(id_table_t *tbl, uint32_t id, uint16_t *out);

/* Write an ID table to a SquashFS image.
   Returns 0 on success. Internally prints error message to stderr. */
int id_table_write(id_table_t *tbl, int outfd, sqfs_super_t *super,
		   compressor_t *cmp);

/* Read an ID table from a SquashFS image.
   Returns 0 on success. Internally prints error messages to stderr. */
int id_table_read(id_table_t *tbl, int fd, sqfs_super_t *super,
		  compressor_t *cmp);

#endif /* ID_TABLE_H */
