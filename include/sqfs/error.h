/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * error.h - This file is part of libsquashfs
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
#ifndef SQFS_ERROR_H
#define SQFS_ERROR_H

/**
 * @file error.h
 *
 * @brief Contains the @ref E_SQFS_ERROR enumerator.
 */

/**
 * @enum E_SQFS_ERROR
 *
 * @brief Error codes that can be returned by various libsquashfs functions.
 */
typedef enum {
	/**
	 * @brief Allocation using malloc or calloc failed (returned NULL).
	 */
	SQFS_ERROR_ALLOC = -1,

	/**
	 * @brief Generic I/O error if a file read or write operation failed.
	 */
	SQFS_ERROR_IO = -2,

	/**
	 * @brief Generic compressor error returned if compressing data failed
	 *        (some kind of internal error) or extracting failed (typically
	 *        means the data is corrupted).
	 */
	SQFS_ERROR_COMPRESSOR = -3,

	/**
	 * @brief An internal error of the "this wasn't supposed to happen"
	 *        kind that cannot easily be mapped to something usefull.
	 */
	SQFS_ERROR_INTERNAL = -4,

	/**
	 * @brief Attempted to read an on-disk data structure that appears to
	 *        be corrupted, i.e. contains obvious non-sense values.
	 */
	SQFS_ERROR_CORRUPTED = -5,

	/**
	 * @brief Attempted to use an unsupported feature (e.g. an unknown
	 *        compressor or xattr type).
	 */
	SQFS_ERROR_UNSUPPORTED = -6,

	/**
	 * @brief Attempted to read a data structure into memory would
	 *        overflow the addressable memory. Usually indicates a
	 *        corrupted or maliciously manipulated SquashFS filesystem.
	 */
	SQFS_ERROR_OVERFLOW = -7,

	/**
	 * @brief Attempted to perform an out-of-bounds read. If this happens
	 *        when following a reference stored in a data structure, it
	 *        usually indicates a corrupted or maliciously manipulated
	 *        SquashFS filesystem.
	 */
	SQFS_ERROR_OUT_OF_BOUNDS = -8,

	/**
	 * @brief Specific error when reading the super block.
	 *
	 * Could not find the magic.
	 */
	SFQS_ERROR_SUPER_MAGIC = -9,

	/**
	 * @brief Specific error when reading the super block.
	 *
	 * The version indicated be the filesystem is not supported.
	 */
	SFQS_ERROR_SUPER_VERSION = -10,

	/**
	 * @brief Specific error when reading or initializing the super block.
	 *
	 * The block size specified is either not a power of 2, or outside the
	 * legal range (4k to 1M).
	 */
	SQFS_ERROR_SUPER_BLOCK_SIZE = -11,

	/**
	 * @brief Expected a directory (inode), found something else instead.
	 *
	 * Generated when trying to resolve a path but a part of the the path
	 * turned out to not be a directory. Also generated when trying to
	 * read directory entries from something that isn't a directory.
	 */
	SQFS_ERROR_NOT_DIR = -12,

	/**
	 * @brief A specified path, or a part of it, does not exist.
	 */
	SQFS_ERROR_NO_ENTRY = -13,

	/**
	 * @brief Detected a hard link loop while walking a filesystem tree.
	 */
	SQFS_ERROR_LINK_LOOP = -14,

	/**
	 * @brief Tried to perform an file operation on something that isn't
	 *        a regular file or a regular file inode.
	 */
	SQFS_ERROR_NOT_FILE = -15,
} E_SQFS_ERROR;

#endif /* SQFS_ERROR_H */
