/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_writer.h - This file is part of libsquashfs
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
#ifndef SQFS_BLOCK_WRITER_H
#define SQFS_BLOCK_WRITER_H

#include "sqfs/predef.h"

/**
 * @file block_writer.h
 *
 * @brief Contains declarations for the @ref sqfs_block_writer_t structure.
 */

/**
 * @struct sqfs_block_writer_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts writing and deduplicating of data and fragment blocks.
 *
 * This object is not copyable, i.e. @ref sqfs_copy will always return NULL.
 */

/**
 * @struct sqfs_block_writer_stats_t
 *
 * @brief Collects run time statistics of the @ref sqfs_block_writer_t
 */
struct sqfs_block_writer_stats_t {
	/**
	 * @brief Holds the size of the structure.
	 *
	 * If a later version of libsquashfs expands this structure, the value
	 * of this field can be used to check at runtime whether the newer
	 * fields are avaialable or not.
	 */
	size_t size;

	/**
	 * @brief Total number of bytes submitted, including blocks that were
	 *        removed by deduplication and not counting any padding.
	 */
	sqfs_u64 bytes_submitted;

	/**
	 * @brief Actual number of bytes written to disk, excluding deduplicated
	 *        blocks and including padding.
	 */
	sqfs_u64 bytes_written;

	/**
	 * @brief Total number of submitted blocks, including ones later
	 *        removed by deduplication.
	 */
	sqfs_u64 blocks_submitted;

	/**
	 * @brief Total number of blocks actually written, excluding
	 *        deduplicated blocks.
	 */
	sqfs_u64 blocks_written;
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a block writer object.
 *
 * @memberof sqfs_block_writer_t
 *
 * @param file A pointer to a file object that data should be appended to.
 * @param devblksz The underlying device block size if output data
 *                 should be aligned.
 * @param flags Currently must be set to 0 or creation will fail.
 *
 * @return A pointer to a new block writer on success, NULL on failure.
 */
SQFS_API sqfs_block_writer_t *sqfs_block_writer_create(sqfs_file_t *file,
						       size_t devblksz,
						       sqfs_u32 flags);

/**
 * @brief Submit a data block to a blokc writer.
 *
 * @memberof sqfs_block_writer_t
 *
 * If the @ref SQFS_BLK_FIRST_BLOCK flag is set, the data block writer
 * memorizes the starting location and block index of the block. If the
 * @ref SQFS_BLK_LAST_BLOCK flag is set, it uses those stored locations
 * to do block deduplication.
 *
 * If the flag @ref SQFS_BLK_ALIGN is set in combination with the
 * @ref SQFS_BLK_FIRST_BLOCK, the file size is padded to a multiple of the
 * device block size before writing. If it is set together with the
 * @ref SQFS_BLK_LAST_BLOCK flag, the padding is added afterwards.
 *
 * @param wr A pointer to a block writer.
 * @param block The block to write to disk next.
 * @param location Returns the location where the block has been written.
 *                 If the @ref SQFS_BLK_LAST_BLOCK flag was set, deduplication
 *                 is performed and this returns the (new) location of the
 *                 first block instead.
 *
 * @return Zero on success, an @ref SQFS_ERROR error on failure.
 */
SQFS_API int sqfs_block_writer_write(sqfs_block_writer_t *wr,
				     sqfs_u32 size, sqfs_u32 checksum,
				     sqfs_u32 flags, const sqfs_u8 *data,
				     sqfs_u64 *location);

/**
 * @brief Get access to a block writers run time statistics.
 *
 * @memberof sqfs_block_writer_t
 *
 * @param wr A pointer to a block writer.
 *
 * @return A pointer to the internal statistics counters.
 */
SQFS_API const sqfs_block_writer_stats_t
*sqfs_block_writer_get_stats(const sqfs_block_writer_t *wr);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_BLOCK_WRITER_H */
