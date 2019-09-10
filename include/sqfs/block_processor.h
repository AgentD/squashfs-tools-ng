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
	 * @brief Set by compressor worker if the block was actually compressed.
	 */
	SQFS_BLK_IS_COMPRESSED = 0x0002,

	/**
	 * @brief Do not calculate block checksum.
	 */
	SQFS_BLK_DONT_CHECKSUM = 0x0004,

	/**
	 * @brief Set by compressor worker if compression failed.
	 */
	SQFS_BLK_COMPRESS_ERROR = 0x0008,

	/**
	 * @brief First user setable block flag.
	 */
	SQFS_BLK_USER = 0x0080
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
	 * @brief User settable file block index.
	 *
	 * Can be used for purposes like indexing the block size table.
	 */
	uint32_t index;

	/**
	 * @brief Arbitary user pointer associated with the block.
	 */
	void *user;

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

/**
 * @brief Signature of a callback function that can is called for each block.
 *
 * Gets called for each processed block. May be called from a different thread
 * than the one that calls enqueue or from the same thread, but only from one
 * thread at a time.
 *
 * Guaranteed to be called on blocks in the order that they are submitted
 * to enqueue.
 *
 * @param user The user pointer passed to @ref sqfs_block_processor_create.
 * @param blk The finished block.
 *
 * @return A non-zero return value is interpreted as fatal error.
 */
typedef int (*sqfs_block_cb)(void *user, sqfs_block_t *blk);

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
 * @param user An arbitrary user pointer to pass to the block callback.
 * @param callback A function to call for each finished data block.
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
						    void *user,
						    sqfs_block_cb callback);

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
 * @brief Convenience function to process a data block.
 *
 * This function actually contains the implementation of what each worker in
 * the block processor actually does to the data blocks.
 *
 * @param block A pointer to a data block.
 * @param cmp A pointer to a compressor to use.
 * @param scratch A pointer to a scratch buffer to user for compressing.
 * @param scratch_size The available size in the scratch buffer.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_process(sqfs_block_t *block, sqfs_compressor_t *cmp,
				uint8_t *scratch, size_t scratch_size);

#ifdef __cplusplus
}
#endif

#endif /* SFQS_BLOCK_PROCESSOR_H */
