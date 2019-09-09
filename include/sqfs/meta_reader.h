/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * meta_reader.h - This file is part of libsquashfs
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
#ifndef SQFS_META_READER_H
#define SQFS_META_READER_H

#include "sqfs/predef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Create a meta data reader using a given compressor to extract data.
   Internally prints error message to stderr on failure.

   Start offset and limit can be specified to do bounds checking against
   a subregion of the filesystem image.
*/
SQFS_API sqfs_meta_reader_t *sqfs_meta_reader_create(sqfs_file_t *file,
						     sqfs_compressor_t *cmp,
						     uint64_t start,
						     uint64_t limit);

SQFS_API void sqfs_meta_reader_destroy(sqfs_meta_reader_t *m);

/* Returns 0 on success. Internally prints to stderr on failure */
SQFS_API int sqfs_meta_reader_seek(sqfs_meta_reader_t *m, uint64_t block_start,
				   size_t offset);

SQFS_API void sqfs_meta_reader_get_position(sqfs_meta_reader_t *m,
					    uint64_t *block_start,
					    size_t *offset);

/* Returns 0 on success. Internally prints to stderr on failure */
SQFS_API int sqfs_meta_reader_read(sqfs_meta_reader_t *m, void *data,
				   size_t size);

/* Returns 0 on success. Internally prints to stderr on failure */
SQFS_API int sqfs_meta_reader_read_dir_header(sqfs_meta_reader_t *m,
					      sqfs_dir_header_t *hdr);

/* Entry can be freed with a single free() call.
   The function internally prints to stderr on failure */
SQFS_API int sqfs_meta_reader_read_dir_ent(sqfs_meta_reader_t *m,
					   sqfs_dir_entry_t **ent);

/* Inode can be freed with a single free() call.
   The function internally prints error message to stderr on failure. */
SQFS_API
int sqfs_meta_reader_read_inode(sqfs_meta_reader_t *ir, sqfs_super_t *super,
				uint64_t block_start, size_t offset,
				sqfs_inode_generic_t **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_META_READER_H */
