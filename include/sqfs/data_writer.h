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
 * @struct sqfs_block_hooks_t
 *
 * @brief A set of hooks for tapping into the data writer.
 *
 * This structure can be registered with an @ref sqfs_data_writer_t and
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
	 * @brief Gets called before writing a block to disk.
	 *
	 * If this is not NULL, it gets called before a block is written to
	 * disk. If the block has the @ref SQFS_BLK_ALLIGN flag set, the
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
	 * disk. If the block has the @ref SQFS_BLK_ALLIGN flag set, the
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
				     uint64_t bytes);

	/**
	 * @brief Gets called before throwing away a fragment that turned out
	 *        to be a duplicate.
	 *
	 * @param user A user pointer.
	 * @param block The data chunk that is about to be merged into the
	 *              fragment block.
	 */
	void (*notify_fragment_discard)(void *user, const sqfs_block_t *block);
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

/**
 * @brief Register a set of hooks to be invoked when writing blocks to disk.
 *
 * @memberof sqfs_data_writer_t
 *
 * @param proc A pointer to a data writer object.
 * @param user_ptr A user pointer to pass to the callbacks.
 * @param hooks A structure containing the hooks.
 */
SQFS_API
void sqfs_data_writer_set_hooks(sqfs_data_writer_t *proc, void *user_ptr,
				const sqfs_block_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif /* SFQS_DATA_WRITER_H */
