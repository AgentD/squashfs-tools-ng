/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir.h - This file is part of libsquashfs
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
#ifndef SQFS_DIR_H
#define SQFS_DIR_H

#include "sqfs/predef.h"

#define SQFS_MAX_DIR_ENT 256

struct sqfs_dir_header_t {
	uint32_t count;
	uint32_t start_block;
	uint32_t inode_number;
};

struct sqfs_dir_entry_t {
	uint16_t offset;
	int16_t inode_diff;
	uint16_t type;
	uint16_t size;
	uint8_t name[];
};

struct sqfs_dir_index_t {
	uint32_t index;
	uint32_t start_block;
	uint32_t size;
	uint8_t name[];
};

#ifdef __cplusplus
extern "C" {
#endif

SQFS_API sqfs_dir_writer_t *sqfs_dir_writer_create(sqfs_meta_writer_t *dm);

SQFS_API void sqfs_dir_writer_destroy(sqfs_dir_writer_t *writer);

SQFS_API int sqfs_dir_writer_begin(sqfs_dir_writer_t *writer);

SQFS_API int sqfs_dir_writer_add_entry(sqfs_dir_writer_t *writer,
				       const char *name,
				       uint32_t inode_num, uint64_t inode_ref,
				       mode_t mode);

SQFS_API int sqfs_dir_writer_end(sqfs_dir_writer_t *writer);

SQFS_API size_t sqfs_dir_writer_get_size(sqfs_dir_writer_t *writer);

SQFS_API uint64_t sqfs_dir_writer_get_dir_reference(sqfs_dir_writer_t *writer);

SQFS_API size_t sqfs_dir_writer_get_index_size(sqfs_dir_writer_t *writer);

SQFS_API int sqfs_dir_writer_write_index(sqfs_dir_writer_t *writer,
					 sqfs_meta_writer_t *im);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_H */
