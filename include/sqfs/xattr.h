/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * xattr.h - This file is part of libsquashfs
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
#ifndef SQFS_XATTR_H
#define SQFS_XATTR_H

#include "sqfs/predef.h"

typedef enum {
	SQFS_XATTR_USER = 0,
	SQFS_XATTR_TRUSTED = 1,
	SQFS_XATTR_SECURITY = 2,

	SQFS_XATTR_FLAG_OOL = 0x100,
	SQFS_XATTR_PREFIX_MASK = 0xFF,
} E_SQFS_XATTR_TYPE;

struct sqfs_xattr_entry_t {
	uint16_t type;
	uint16_t size;
	uint8_t key[];
};

struct sqfs_xattr_value_t {
	uint32_t size;
	uint8_t value[];
};

struct sqfs_xattr_id_t {
	uint64_t xattr;
	uint32_t count;
	uint32_t size;
};

struct sqfs_xattr_id_table_t {
	uint64_t xattr_table_start;
	uint32_t xattr_ids;
	uint32_t unused;
};

#ifdef __cplusplus
extern "C" {
#endif

/* Get a prefix string from the ID or NULL if unknown */
SQFS_API const char *sqfs_get_xattr_prefix(E_SQFS_XATTR_TYPE id);

/* Get id from xattr key prefix or -1 if not supported */
SQFS_API int sqfs_get_xattr_prefix_id(const char *key);

/* Check if a given xattr key can be encoded in squashfs at all. */
SQFS_API bool sqfs_has_xattr(const char *key);

SQFS_API int sqfs_xattr_reader_load_locations(sqfs_xattr_reader_t *xr);

SQFS_API void sqfs_xattr_reader_destroy(sqfs_xattr_reader_t *xr);

SQFS_API sqfs_xattr_reader_t *sqfs_xattr_reader_create(sqfs_file_t *file,
						       sqfs_super_t *super,
						       sqfs_compressor_t *cmp);

SQFS_API int sqfs_xattr_reader_get_desc(sqfs_xattr_reader_t *xr, uint32_t idx,
					sqfs_xattr_id_t *desc);

SQFS_API int sqfs_xattr_reader_seek_kv(sqfs_xattr_reader_t *xr,
				       const sqfs_xattr_id_t *desc);

SQFS_API
int sqfs_xattr_reader_read_value(sqfs_xattr_reader_t *xr,
				 const sqfs_xattr_entry_t *key,
				 sqfs_xattr_value_t **val_out);

SQFS_API
int sqfs_xattr_reader_read_key(sqfs_xattr_reader_t *xr,
			       sqfs_xattr_entry_t **key_out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_XATTR_H */
