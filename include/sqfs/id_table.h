/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * id_table.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_ID_TABLE_H
#define SQFS_ID_TABLE_H

#include "config.h"

#include <stdint.h>
#include <stddef.h>

#include "compress.h"

typedef struct sqfs_id_table_t sqfs_id_table_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Prints error message to stderr on failure. */
sqfs_id_table_t *sqfs_id_table_create(void);

void sqfs_id_table_destroy(sqfs_id_table_t *tbl);

/* Resolve a 32 bit to a 16 bit table index.
   Returns 0 on success. Internally prints errors to stderr. */
int sqfs_id_table_id_to_index(sqfs_id_table_t *tbl, uint32_t id, uint16_t *out);

/* Write an ID table to a SquashFS image.
   Returns 0 on success. Internally prints error message to stderr. */
int sqfs_id_table_write(sqfs_id_table_t *tbl, int outfd, sqfs_super_t *super,
			compressor_t *cmp);

/* Read an ID table from a SquashFS image.
   Returns 0 on success. Internally prints error messages to stderr. */
int sqfs_id_table_read(sqfs_id_table_t *tbl, int fd, sqfs_super_t *super,
		       compressor_t *cmp);

int sqfs_id_table_index_to_id(const sqfs_id_table_t *tbl, uint16_t index,
			      uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_ID_TABLE_H */
