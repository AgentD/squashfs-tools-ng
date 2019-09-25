/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * data_writer.h - This file is part of libsquashfs
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
#ifndef SFQS_DATA_WRITER_H
#define SFQS_DATA_WRITER_H

#include "sqfs/predef.h"

/**
 * @file data_writer.h
 *
 * @brief Contains declarations for the data block processor.
 */

/**
 * @struct sqfs_data_writer_t
 *
 * @brief Encapsulates a thread pool based block processor.
 *
 * The actual implementation may even be non-threaded, depending on the
 * operating system and compile configuration.
 *
 * Either way, the instantiated object processes data blocks that can be
 * enqueued through @ref sqfs_data_writer_enqueue. The completed blocks
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
	 * @brief Indicates that a block is a tail end of a file and the block
	 *        processor should take care of fragment packing and accounting.
	 */
	SQFS_BLK_IS_FRAGMENT = 0x0010,

	/**
	 * @brief Set by the block processor on fragment blocks that
	 *        it generates.
	 */
	SQFS_BLK_FRAGMENT_BLOCK = 0x4000,

	/**
	 * @brief Set by compressor worker if the block was actually compressed.
	 */
	SQFS_BLK_IS_COMPRESSED = 0x8000,

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
 * @brief Create a data block writer.
 *
 * @memberof sqfs_data_writer_t
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
 * @param devblksz File can optionally be allgined to device block size. This
 *                 specifies the desired allignment.
 * @param file The output file to write the finished blocks to.
 *
 * @return A pointer to a data writer object on success, NULL on allocation
 *         failure or on failure to create and initialize the worker threads.
 */
SQFS_API
sqfs_data_writer_t *sqfs_data_writer_create(size_t max_block_size,
					    sqfs_compressor_t *cmp,
					    unsigned int num_workers,
					    size_t max_backlog,
					    size_t devblksz,
					    sqfs_file_t *file);

/**
 * @brief Destroy a data writer and free all memory used by it.
 *
 * @memberof sqfs_data_writer_t
 *
 * @param proc A pointer to a data writer object.
 */
SQFS_API void sqfs_data_writer_destroy(sqfs_data_writer_t *proc);

/**
 * @brief Add a block to be processed.
 *
 * @memberof sqfs_data_writer_t
 *
 * The function takes over ownership of the submitted block. It is freed after
 * processing is done and it is written to disk.
 *
 * @param proc A pointer to a data writer object.
 * @param block A pointer to a block to enqueue.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure. Depending on
 *         the implementation used, the failure could have been caused by
 *         processing of a block that was submitted earlier.
 */
SQFS_API int sqfs_data_writer_enqueue(sqfs_data_writer_t *proc,
				      sqfs_block_t *block);

/**
 * @brief Wait for the works to finish and finally flush the current
 *        fragment block.
 *
 * @memberof sqfs_data_writer_t
 *
 * @param proc A pointer to a block processor object.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure. The failure
 *         return value can either be an error encountered during enqueueing,
 *         processing or writing to disk.
 */
SQFS_API int sqfs_data_writer_finish(sqfs_data_writer_t *proc);

/**
 * @brief Write the completed fragment table to disk.
 *
 * @memberof sqfs_data_writer_t
 *
 * Call this after producing the inode and directory table to generate
 * the fragment table for the squashfs image.
 *
 * @param proc A pointer to a data writer object.
 * @param super A pointer to a super block to write information about the
 *              fragment table to.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API
int sqfs_data_writer_write_fragment_table(sqfs_data_writer_t *proc,
					  sqfs_super_t *super);

#ifdef __cplusplus
}
#endif

#endif /* SFQS_DATA_WRITER_H */
