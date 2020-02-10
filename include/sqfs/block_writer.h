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
 * @brief Abstracts writing and deduplicating of data and fragment blocks.
 */

/**
 * @struct sqfs_block_hooks_t
 *
 * @brief A set of hooks for tapping into the data writer.
 *
 * This structure can be registered with an @ref sqfs_block_writer_t and
 * contains function pointers that will be called during various stages
 * when writing data to disk.
 *
 * The callbacks can not only be used for accounting but may also write extra
 * data to the output file or make modifications to the blocks before they are
 * writtien.
 *
 * The callbacks can be individually set to NULL to disable them.
 */
struct sqfs_block_hooks_t {
	/**
	 * @brief Set this to the size of the struct.
	 *
	 * This is required for future expandabillity while maintaining ABI
	 * compatibillity. At the current time, the implementation of
	 * @ref sqfs_block_writer_set_hooks rejects any hook struct where
	 * this isn't the exact size. If new hooks are added in the future,
	 * the struct grows and the future implementation can tell by the size
	 * whether the application uses the new version or the old one.
	 */
	size_t size;

	/**
	 * @brief Gets called before writing a block to disk.
	 *
	 * If this is not NULL, it gets called before a block is written to
	 * disk. If the block has the @ref SQFS_BLK_ALIGN flag set, the
	 * function is called before padding the file.
	 *
	 * The function may modify the block itself or write data to the file.
	 * which is taken into account when padding the file.
	 *
	 * @param user A user pointer.
	 * @param block The block that is about to be written.
	 * @param file The file that the block will be written to.
	 */
	void (*pre_block_write)(void *user, sqfs_block_t *block,
				sqfs_file_t *file);

	/**
	 * @brief Gets called after writing a block to disk.
	 *
	 * If this is not NULL, it gets called after a block is written to
	 * disk. If the block has the @ref SQFS_BLK_ALIGN flag set, the
	 * function is called before padding the file.
	 *
	 * Modifying the block is rather pointless, but the function may
	 * write data to the file which is taken into account when padding
	 * the file.
	 *
	 * @param user A user pointer.
	 * @param block The block that is about to be written.
	 * @param file The file that the block was written to.
	 */
	void (*post_block_write)(void *user, const sqfs_block_t *block,
				 sqfs_file_t *file);

	/**
	 * @brief Gets called before storing a fragment in a fragment block.
	 *
	 * The function can modify the block before it is stored.
	 *
	 * @param user A user pointer.
	 * @param block The data chunk that is about to be merged into the
	 *              fragment block.
	 */
	void (*pre_fragment_store)(void *user, sqfs_block_t *block);

	/**
	 * @brief Gets called if block deduplication managed to get
	 *        rid of the data blocks of a file.
	 *
	 * @param user A user pointer.
	 * @param count The number of blocks that have been erased.
	 * @param bytes The number of bytes that have been erased. Includes
	 *              potential padding before and after the end.
	 */
	void (*notify_blocks_erased)(void *user, size_t count,
				     sqfs_u64 bytes);

	/**
	 * @brief Gets called before throwing away a fragment that turned out
	 *        to be a duplicate.
	 *
	 * @param user A user pointer.
	 * @param block The data chunk that is about to be merged into the
	 *              fragment block.
	 */
	void (*notify_fragment_discard)(void *user, const sqfs_block_t *block);

	/**
	 * @brief Gets called before writing a block of padding bytes to disk.
	 *
	 * @param user A user pointer.
	 * @param block The padding bytes that are about to be written.
	 * @param count The number of padding bytes in the block.
	 */
	void (*prepare_padding)(void *user, sqfs_u8 *block, size_t count);
};

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

SQFS_API sqfs_block_writer_t *sqfs_block_writer_create(sqfs_file_t *file,
						       size_t devblksz,
						       sqfs_u32 flags);

SQFS_API int sqfs_block_writer_set_hooks(sqfs_block_writer_t *wr,
					 void *user_ptr,
					 const sqfs_block_hooks_t *hooks);

SQFS_API void sqfs_block_writer_destroy(sqfs_block_writer_t *wr);

SQFS_API int sqfs_block_writer_write(sqfs_block_writer_t *wr,
				     sqfs_block_t *block,
				     sqfs_u64 *location);

SQFS_API const sqfs_block_writer_stats_t
*sqfs_block_writer_get_stats(const sqfs_block_writer_t *wr);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_BLOCK_WRITER_H */
