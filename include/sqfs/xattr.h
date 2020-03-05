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

/**
 * @file xattr.h
 *
 * @brief Contains on-disk data structures for storing extended attributes.
 */

/**
 * @enum SQFS_XATTR_TYPE
 *
 * Used by @ref sqfs_xattr_entry_t to encodes the xattr prefix.
 */
typedef enum {
	SQFS_XATTR_USER = 0,
	SQFS_XATTR_TRUSTED = 1,
	SQFS_XATTR_SECURITY = 2,

	SQFS_XATTR_FLAG_OOL = 0x100,
	SQFS_XATTR_PREFIX_MASK = 0xFF,
} SQFS_XATTR_TYPE;

/**
 * @struct sqfs_xattr_entry_t
 *
 * @brief On-disk data structure that holds a single xattr key
 *
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_entry_t {
	/**
	 * @brief Encodes the prefix of the key
	 *
	 * A @ref SQFS_XATTR_TYPE value. If the @ref SQFS_XATTR_FLAG_OOL is
	 * set, the value that follows is not actually a string but a 64 bit
	 * reference to the location where the value is actually stored.
	 */
	sqfs_u16 type;

	/**
	 * @brief The size in bytes of the suffix string that follows
	 */
	sqfs_u16 size;
	sqfs_u8 key[];
};

/**
 * @struct sqfs_xattr_value_t
 *
 * @brief On-disk data structure that holds a single xattr value
 *
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_value_t {
	/**
	 * @brief The exact size in bytes of the value that follows
	 */
	sqfs_u32 size;
	sqfs_u8 value[];
};

/**
 * @struct sqfs_xattr_id_t
 *
 * @brief On-disk data structure that describes a set of key-value pairs
 *
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_id_t {
	/**
	 * @brief Location of the first key-value pair
	 *
	 * This is a reference, i.e. the bits 16 to 48 hold an offset that is
	 * added to xattr_table_start from @ref sqfs_xattr_id_table_t to get
	 * the location of a meta data block that contains the first key-value
	 * pair. The lower 16 bits store an offset into the uncompressed meta
	 * data block.
	 */
	sqfs_u64 xattr;

	/**
	 * @brief Number of consecutive key-value pairs
	 */
	sqfs_u32 count;

	/**
	 * @brief Total size of the uncompressed key-value pairs in bytes,
	 *        including data structures used to encode them.
	 */
	sqfs_u32 size;
};

/**
 * @struct sqfs_xattr_id_table_t
 *
 * @brief On-disk data structure that the super block points to
 *
 * Indicates the locations of the xattr key-value pairs and descriptor array.
 * See @ref sqfs_xattr_reader_t for an overview how SquashFS stores extended
 * attributes on disk.
 */
struct sqfs_xattr_id_table_t {
	/**
	 * @brief The location of the first meta data block holding the key
	 *        value pairs.
	 */
	sqfs_u64 xattr_table_start;

	/**
	 * @brief The total number of descriptors (@ref sqfs_xattr_id_t)
	 */
	sqfs_u32 xattr_ids;

	/**
	 * @brief Unused, alwayas set this to 0 when writing!
	 */
	sqfs_u32 unused;

	/**
	 * @brief Holds the locations of the meta data blocks that contain the
	 *        @ref sqfs_xattr_id_t descriptor array.
	 */
	sqfs_u64 locations[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Resolve an xattr identifier to the coresponding prefix
 *
 * Like many file systems, SquashFS stores xattrs be cutting off the common
 * prefix of the key string and storing an enumerator instead to save memory.
 *
 * This function takes an @ref SQFS_XATTR_TYPE identifier and returns the
 * coresponding prefix string, including the '.' at the end that separates
 * the prefix from the rest of the key.
 */
SQFS_API const char *sqfs_get_xattr_prefix(SQFS_XATTR_TYPE id);

/**
 * @brief Resolve an xattr prefix into an identifier
 *
 * Like many file systems, SquashFS stores xattrs be cutting off the common
 * prefix of the key string and storing an enumerator instead to save memory.
 *
 * This function takes a key and finds the enumerator value that represents
 * its prefix. An error value is returned if the given prefix isn't supported.
 *
 * @return On success an @ref SQFS_XATTR_TYPE. If not supported, the
 *         @ref SQFS_ERROR_UNSUPPORTED error code.
 */
SQFS_API int sqfs_get_xattr_prefix_id(const char *key);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_XATTR_H */
