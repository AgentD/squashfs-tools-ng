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
 * @enum E_SQFS_FILE_OPEN_FLAGS
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

	SQFS_FILE_OPEN_ALL_FLAGS = 0x03,
} E_SQFS_FILE_OPEN_FLAGS;

/**
 * @interface sqfs_file_t
 *
 * @brief Abstracts file I/O to make it easy to embedd SquashFS.
 */
struct sqfs_file_t {
	/**
	 * @brief Close the file and destroy the interface implementation.
	 *
	 * @param file A pointer to the file object.
	 */
	void (*destroy)(sqfs_file_t *file);

	/**
	 * @brief Read a chunk of data from an absolute position.
	 *
	 * @param file A pointer to the file object.
	 * @param offset An absolute offset to read data from.
	 * @param buffer A pointer to a buffer to copy the data to.
	 * @param size The number of bytes to read from the file.
	 *
	 * @return Zero on success, an @ref E_SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*read_at)(sqfs_file_t *file, uint64_t offset,
		       void *buffer, size_t size);

	/**
	 * @brief Write a chunk of data at an absolute position.
	 *
	 * @param file A pointer to the file object.
	 * @param offset An absolute offset to write data to.
	 * @param buffer A pointer to a buffer to write to the file.
	 * @param size The number of bytes to write from the buffer.
	 *
	 * @return Zero on success, an @ref E_SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*write_at)(sqfs_file_t *file, uint64_t offset,
			const void *buffer, size_t size);

	/**
	 * @brief Get the number of bytes currently stored in the file.
	 *
	 * @param file A pointer to the file object.
	 */
	uint64_t (*get_size)(const sqfs_file_t *file);

	/**
	 * @brief Extend or shrink a file to a specified size.
	 *
	 * @param file A pointer to the file object.
	 * @param size The new capacity of the file in bytes.
	 *
	 * @return Zero on success, an @ref E_SQFS_ERROR identifier on failure
	 *         that the data structures in libsquashfs that use this return
	 *         directly to the caller.
	 */
	int (*truncate)(sqfs_file_t *file, uint64_t size);
};

/**
 * @struct sqfs_sparse_map_t
 *
 * @brief Describes the layout of a sparse file.
 *
 * This structure is part of a linked list that indicates where the actual
 * data is located in a sparse file.
 */
struct sqfs_sparse_map_t {
	sqfs_sparse_map_t *next;
	uint64_t offset;
	uint64_t count;
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
 * preserves the value in errno indicating the underlying problem.
 *
 * @param filename The name of the file to open.
 * @param flags A set of @ref E_SQFS_FILE_OPEN_FLAGS.
 *
 * @return A pointer to a file object on success, NULL on allocation failure,
 *         failure to open the file or if an unknown flag was set.
 */
SQFS_API sqfs_file_t *sqfs_open_file(const char *filename, int flags);

/**
 * @brief Read a chunk from a file and turn it into a block that can be
 *        fed to a block processor.
 *
 * @member sqfs_file_t
 *
 * @param file A pointer to a file implementation.
 * @param offset A byte offset into the file.
 * @param size The number of bytes to read, starting at the given offset.
 * @param inode The inode pointer to set for the block.
 * @param flags The flags to store in the newly created block.
 * @param out Returns a pointer to a block on success.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR identifier on failure.
 */
SQFS_API int sqfs_file_create_block(sqfs_file_t *file, uint64_t offset,
				    size_t size, sqfs_inode_generic_t *inode,
				    uint32_t flags, sqfs_block_t **out);

/**
 * @brief Read a chunk from a condensed version of a sparse file and turn it
 *        into a block that can be fed to a block processor.
 *
 * @member sqfs_file_t
 *
 * This function works on condensed sparse files, i.e. a sparse file that had
 * its holdes removed. The given mapping describes the original data region
 * that are actually packed next to each other. The function emulates the
 * orignal sparse file by zero-initializing the block data, then figuring
 * out which regions overlap the block, working out their physical location and
 * stitching the block together.
 *
 * @param file A pointer to a file implementation.
 * @param offset A byte offset into the file.
 * @param size The number of bytes to read, starting at the given offset.
 * @param inode The inode pointer to set for the block.
 * @param flags The flags to store in the newly created block.
 * @param map Describes the data regions of the original sparse file.
 * @param out Returns a pointer to a block on success.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR identifier on failure.
 */
SQFS_API int sqfs_file_create_block_dense(sqfs_file_t *file, uint64_t offset,
					  size_t size,
					  sqfs_inode_generic_t *inode,
					  uint32_t flags,
					  const sqfs_sparse_map_t *map,
					  sqfs_block_t **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_IO_H */
