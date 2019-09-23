/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.h - This file is part of libsquashfs
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
#ifndef SFQS_BLOCK_PROCESSOR_H
#define SFQS_BLOCK_PROCESSOR_H

#include "sqfs/predef.h"

/**
 * @file block_processor.h
 *
 * @brief Contains declarations for the data block processor.
 */

/**
 * @struct sqfs_block_processor_t
 *
 * @brief Encapsulates a thread pool based block processor.
 *
 * The actual implementation may even be non-threaded, depending on the
 * operating system and compile configuration.
 *
 * Either way, the instantiated object processes data blocks that can be
 * enqueued through @ref sqfs_block_processor_enqueue. The completed blocks
 * (compressed and checksumed) are dequeued in the same order and a callback
 * is called for each one.
 */

/**
 * @enum E_SQFS_BLK_FLAGS
 *
 * @brief Generic flags that tell the processor what to do with a block and
 *        flags that the processor sets when it is done with a block.
 */
typedef enum {
	/**
	 * @brief Only calculate checksum, do NOT compress the data.
	 */
	SQFS_BLK_DONT_COMPRESS = 0x0001,

	/**
	 * @brief Indicates that an equeued block is the first block of a file.
	 */
	SQFS_BLK_FIRST_BLOCK = 0x0002,

	/**
	 * @brief Indicates that an equeued block is the last block of a file.
	 */
	SQFS_BLK_LAST_BLOCK = 0x0004,

	/**
	 * @brief Allign the block on disk to device block size.
	 *
	 * If set in combination with @ref SQFS_BLK_FIRST_BLOCK, the output
	 * file is padded to a multiple of the device block size before writing
	 * the block.
	 *
	 * If used with @ref SQFS_BLK_LAST_BLOCK, the output file is padded
	 * after writing the block.
	 */
	SQFS_BLK_ALLIGN = 0x0008,

	/**
	 * @brief Indicates that a block is not part of a file but contains
	 *        file tail ends and an entry in the fragment table has to be
	 *        added.
	 */
	SQFS_BLK_FRAGMENT_BLOCK = 0x0010,

	/**
	 * @brief Set by compressor worker if the block was actually compressed.
	 */
	SQFS_BLK_IS_COMPRESSED = 0x4000,

	/**
	 * @brief Set by compressor worker if compression failed.
	 */
	SQFS_BLK_COMPRESS_ERROR = 0x8000,

	/**
	 * @brief The combination of all flags that are user settable.
	 */
	SQFS_BLK_USER_SETTABLE_FLAGS = 0x001F,
} E_SQFS_BLK_FLAGS;

/**
 * @struct sqfs_block_t
 *
 * @brief Encapsulates a chunk of data to be processed by the block processor.
 */
struct sqfs_block_t {
	/**
	 * @brief Used internally, existing value is ignored and overwritten
	 *        when enqueueing a block.
	 */
	sqfs_block_t *next;

	/**
	 * @brief Used internally, existing value is ignored and overwritten
	 *        when enqueueing a block.
	 */
	uint32_t sequence_number;

	/**
	 * @brief Size of the data area.
	 */
	uint32_t size;

	/**
	 * @brief Checksum of the input data.
	 */
	uint32_t checksum;

	/**
	 * @brief Data block index within the inode.
	 */
	uint32_t index;

	/**
	 * @brief The squashfs inode related to this block.
	 */
	sqfs_inode_generic_t *inode;

	/**
	 * @brief User settable flag field.
	 *
	 * A combination of @ref E_SQFS_BLK_FLAGS and custom, user
	 * settable flags.
	 */
	uint32_t flags;

	/**
	 * @brief Raw data to be processed.
	 */
	uint8_t data[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a block processor.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param max_block_size The maximum size of a data block. Required for the
 *                       internal scratch buffer used for compressing data.
 * @param cmp A pointer to a compressor. If multiple worker threads are used,
 *            the deep copy function of the compressor is used to create
 *            several instances that don't interfere with each other.
 * @param num_workers The number of worker threads to create.
 * @param max_backlog The maximum number of blocks currently in flight. When
 *                    trying to add more, enqueueing blocks until the in-flight
 *                    block count drops below the threshold.
 * @param devblksz The device block size to allign files to if they have the
 *                 apropriate flag set.
 * @param file The output file to write the finished blocks to.
 *
 * @return A pointer to a block processor object on success, NULL on allocation
 *         failure or on failure to create the worker threads and
 *         synchronisation primitives.
 */
SQFS_API
sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    size_t devblksz,
						    sqfs_file_t *file);

/**
 * @brief Destroy a block processor and free all memory used by it.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param proc A pointer to a block processor object.
 */
SQFS_API void sqfs_block_processor_destroy(sqfs_block_processor_t *proc);

/**
 * @brief Add a block to be processed.
 *
 * @memberof sqfs_block_processor_t
 *
 * The function takes over ownership of the submitted block. It is freed after
 * processing and calling the block callback.
 *
 * @note Even on failure, the workers may still be running and you should still
 *       call @ref sqfs_block_processor_finish before cleaning up.
 *
 * @param proc A pointer to a block processor object.
 * @param block A poitner to a block to enqueue.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure. Depending on
 *         the implementation used, the failure return value can actually come
 *         directly from the block callback.
 */
SQFS_API int sqfs_block_processor_enqueue(sqfs_block_processor_t *proc,
					  sqfs_block_t *block);

/**
 * @brief Wait for the workers to finish all in-flight data blocks.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param proc A pointer to a block processor object.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure. The failure
 *         return value can either be an error encountered during enqueueing,
 *         processing or a failure return status from the block callback.
 */
SQFS_API int sqfs_block_processor_finish(sqfs_block_processor_t *proc);

/**
 * @brief Write the completed fragment table to disk.
 *
 * @memberof sqfs_block_processor_t
 *
 * Call this after producing the inode and directory table to generate
 * the fragment table for the squashfs image.
 *
 * @param proc A pointer to a block processor object.
 * @param super A pointer to a super block to write information about the
 *              fragment table to.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure. The failure
 *         return value can either be an error encountered during enqueueing,
 *         processing or a failure return status from the block callback.
 */
SQFS_API
int sqfs_block_processor_write_fragment_table(sqfs_block_processor_t *proc,
					      sqfs_super_t *super);

#ifdef __cplusplus
}
#endif

#endif /* SFQS_BLOCK_PROCESSOR_H */
