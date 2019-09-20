/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * data_reader.h - This file is part of libsquashfs
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
#ifndef SQFS_DATA_READER_H
#define SQFS_DATA_READER_H

#include "sqfs/predef.h"

/**
 * @file data_reader.h
 *
 * @brief Contains declarations for the @ref sqfs_data_reader_t.
 */

/**
 * @struct sqfs_data_reader_t
 *
 * @brief Abstracts access to data blocks stored in a SquashFS image.
 *
 * A SquashFS image can contain a series of file data blocks between the
 * super block and the inode table. Blocks may or may not be compressed.
 * Data chunks that are smaller than the block size indicated by the super
 * block (such as the final chunk of a file or an entire file that is smaller
 * than a signle block) can be grouped in a single fragment block.
 *
 * Regular file inodes referre to the location of the first block and store a
 * sequence of block sizes for all consequitve blocks, as well as a fragment
 * index and fragment offset which is resolved through a fragment table.
 *
 * The data reader abstracts all of this away in a simple interface that allows
 * reading file data through an inode description and a location in the file.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a data reader instance.
 *
 * @memberof sqfs_data_reader_t
 *
 * @param file A file interface through which to access the
 *             underlying filesystem image.
 * @param block_size The data block size from the super block.
 * @param cmp A compressor to use for uncompressing blocks read from disk.
 *
 * @return A pointer to a new data reader object. NULL means
 *         allocation failure.
 */
SQFS_API sqfs_data_reader_t *sqfs_data_reader_create(sqfs_file_t *file,
						     size_t block_size,
						     sqfs_compressor_t *cmp);

/**
 * @brief Destroy a data reader instance and free all memory used by it.
 *
 * @memberof sqfs_data_reader_t
 *
 * @param data A pointer to a data reader object.
 */
SQFS_API void sqfs_data_reader_destroy(sqfs_data_reader_t *data);

/**
 * @brief Read and decode the fragment table from disk.
 *
 * @memberof sqfs_data_reader_t
 *
 * @param data A pointer to a data reader object.
 * @param super A pointer to the super block.
 *
 * @return Zero on succcess, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_data_reader_load_fragment_table(sqfs_data_reader_t *data,
						  const sqfs_super_t *super);

/**
 * @brief Get the tail end of a file.
 *
 * @memberof sqfs_data_reader_t
 *
 * @param data A pointer to a data reader object.
 * @param inode A pointer to the inode describing the file.
 * @param out Returns a pointer to the tail block of the file.
 *
 * @return Zero on succcess, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_data_reader_get_fragment(sqfs_data_reader_t *data,
					   const sqfs_inode_generic_t *inode,
					   sqfs_block_t **out);

/**
 * @brief Get a full sized data block of a file by block index.
 *
 * @memberof sqfs_data_reader_t
 *
 * @param data A pointer to a data reader object.
 * @param inode A pointer to the inode describing the file.
 * @param index The block index in the inodes block list.
 * @param out Returns a pointer to the data block.
 *
 * @return Zero on succcess, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_data_reader_get_block(sqfs_data_reader_t *data,
					const sqfs_inode_generic_t *inode,
					size_t index, sqfs_block_t **out);

/**
 * @brief A simple UNIX-read-like function to read data from a file.
 *
 * @memberof sqfs_data_reader_t
 *
 * This function acts like the read system call in a Unix-like OS. It takes
 * care of reading accross data blocks and fragment internally, using a
 * data and fragment block cache.
 *
 * @param data A pointer to a data reader object.
 * @param inode A pointer to the inode describing the file.
 * @param offset An arbitrary byte offset into the uncompressed file.
 * @param buffer Returns the data read from the file.
 * @param size The number of uncompressed bytes to read from the given offset.
 *
 * @return The number of bytes read on succcess, zero if attempting to read
 *         past the end of the file and a negative @ref E_SQFS_ERROR value
 *         on failure.
 */
SQFS_API ssize_t sqfs_data_reader_read(sqfs_data_reader_t *data,
				       const sqfs_inode_generic_t *inode,
				       uint64_t offset, void *buffer,
				       size_t size);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DATA_READER_H */
