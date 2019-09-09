/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * id_table.h - This file is part of libsquashfs
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef SQFS_ID_TABLE_H
#define SQFS_ID_TABLE_H

#include "sqfs/predef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Prints error message to stderr on failure. */
SQFS_API sqfs_id_table_t *sqfs_id_table_create(void);

SQFS_API void sqfs_id_table_destroy(sqfs_id_table_t *tbl);

/* Resolve a 32 bit to a 16 bit table index.
   Returns 0 on success. Internally prints errors to stderr. */
SQFS_API int sqfs_id_table_id_to_index(sqfs_id_table_t *tbl, uint32_t id,
				       uint16_t *out);

/* Write an ID table to a SquashFS image.
   Returns 0 on success. Internally prints error message to stderr. */
SQFS_API int sqfs_id_table_write(sqfs_id_table_t *tbl, sqfs_file_t *file,
				 sqfs_super_t *super, sqfs_compressor_t *cmp);

/* Read an ID table from a SquashFS image.
   Returns 0 on success. Internally prints error messages to stderr. */
SQFS_API int sqfs_id_table_read(sqfs_id_table_t *tbl, sqfs_file_t *file,
				sqfs_super_t *super, sqfs_compressor_t *cmp);

SQFS_API int sqfs_id_table_index_to_id(const sqfs_id_table_t *tbl,
				       uint16_t index, uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_ID_TABLE_H */
