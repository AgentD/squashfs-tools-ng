/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * io.h - This file is part of libsquashfs
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
#ifndef SQFS_IO_H
#define SQFS_IO_H

#include "sqfs/predef.h"

/**
 * @file io.h
 *
 * @brief Contains the @ref sqfs_file_t interface for abstracting file I/O
 */

/**
 * @enum SQFS_FILE_OPEN_FLAGS
 *
 * @brief Flags for @ref sqfs_open_file
 */
typedef enum {
	/**
	 * @brief If set, access the file for reading only
	 *
	 * If not set, the file is expected to have a zero size after opening
	 * which can be grown with successive writes to end of the file.
	 *
	 * Opening an existing file with this flag cleared results in failure,
	 * unless the @ref SQFS_FILE_OPEN_OVERWRITE flag is also set.
	 */
	SQFS_FILE_OPEN_READ_ONLY = 0x01,

	/**
	 * @brief If the read only flag is not set, overwrite any
	 *        existing file.
	 *
	 * If the file alrady exists, it is truncated to zero bytes size and
	 * overwritten.
	 */
	SQFS_FILE_OPEN_OVERWRITE = 0x02,

	/**
	 * @brief If set, do not try to apply any character set transformations
	 *        to the file path.
	 *
	 * This flag instructs the @ref sqfs_open_file API to pass the file
	 * path to the OS dependent API as-is and not to attempt any character
	 * set transformation. By default, if the underlying OS has a concept
	 * of locale dependent file paths, the input path is UTF-8 and it is
	 * transformed as needed, in order to retain a level of portabillity
	 * and sanity.
	 *
	 * This flag currently only affects the Windows implementation. On
	 * Unix-like systems, the path is always passed to the OS API as-is
	 * and this flag has no effect.
	 *
	 * On Windows, the input path is converted from UTF-8 to UTF-16 and
	 * then passed on to the wide-char based file API. If this flag is set,
	 * the path is used as-is and passed on to the 8-bit codepage-whatever
	 * API instead.
	 */
	SQFS_FILE_OPEN_NO_CHARSET_XFRM = 0x04,

	SQFS_FILE_OPEN_ALL_FLAGS = 0x07,
} SQFS_FILE_OPEN_FLAGS;

/**
 * @interface sqfs_file_t
 *
 * @extends sqfs_object_t
 *
 * @brief Abstracts file I/O to make it easy to embedd SquashFS.
 *
 * Files are only copyable if they are read only, i.e. if a file has been
 * opened with write access, @ref sqfs_copy will always return NULL. The
 * other data types inside libsquashfs assume this to hold for all
 * implementations of this interface.
 */
struct sqfs_file_t {
	sqfs_object_t base;

	/**
	 * @brief Read a chunk of data from an absolute position.
	 *
	 * @param file A pointer to the file object.
	 * @param offset An absolute offset to read data from.
	 * @param buffer A pointer to a buffer to copy the data to.
	 * @param size The number of bytes to read from the file.
	 *
	 * @return Zero on success, an @ref SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*read_at)(sqfs_file_t *file, sqfs_u64 offset,
		       void *buffer, size_t size);

	/**
	 * @brief Write a chunk of data at an absolute position.
	 *
	 * @param file A pointer to the file object.
	 * @param offset An absolute offset to write data to.
	 * @param buffer A pointer to a buffer to write to the file.
	 * @param size The number of bytes to write from the buffer.
	 *
	 * @return Zero on success, an @ref SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*write_at)(sqfs_file_t *file, sqfs_u64 offset,
			const void *buffer, size_t size);

	/**
	 * @brief Get the number of bytes currently stored in the file.
	 *
	 * @param file A pointer to the file object.
	 */
	sqfs_u64 (*get_size)(const sqfs_file_t *file);

	/**
	 * @brief Extend or shrink a file to a specified size.
	 *
	 * @param file A pointer to the file object.
	 * @param size The new capacity of the file in bytes.
	 *
	 * @return Zero on success, an @ref SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*truncate)(sqfs_file_t *file, sqfs_u64 size);
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open a file through the operating systems filesystem API
 *
 * This function internally creates an instance of a reference implementation
 * of the @ref sqfs_file_t interface that uses the operating systems native
 * API for file I/O.
 *
 * On Unix-like systems, if the open call fails, this function makes sure to
 * preserves the value in errno indicating the underlying problem. Similarly,
 * on Windows, the implementation tries to preserve the GetLastError value.
 *
 * @param filename The name of the file to open.
 * @param flags A set of @ref SQFS_FILE_OPEN_FLAGS.
 *
 * @return A pointer to a file object on success, NULL on allocation failure,
 *         failure to open the file or if an unknown flag was set.
 */
SQFS_API sqfs_file_t *sqfs_open_file(const char *filename, sqfs_u32 flags);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_IO_H */
