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
 * @brief Abstracts generating of file data and fragment blocks.
 *
 * This data structure provides a simple begin/append/end interface
 * to generate file data blocks (see @ref sqfs_block_processor_begin_file,
 * @ref sqfs_block_processor_append and @ref sqfs_block_processor_end
 * respectively).
 *
 * Internally it takes care of partitioning data in the correct block sizes,
 * adding tail-ens to fragment blocks, compressing the data, deduplicating data
 * and finally writing it to disk.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a data block writer.
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
 * @param wr A block writer to send to finished blocks to.
 * @param tbl A fragment table to use for storing fragment and fragment block
 *            locations.
 *
 * @return A pointer to a data writer object on success, NULL on allocation
 *         failure or on failure to create and initialize the worker threads.
 */
SQFS_API
sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl);

/**
 * @brief Destroy a data writer and free all memory used by it.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param proc A pointer to a data writer object.
 */
SQFS_API void sqfs_block_processor_destroy(sqfs_block_processor_t *proc);

/**
 * @brief Start writing a file.
 *
 * @memberof sqfs_block_processor_t
 *
 * After calling this function, call @ref sqfs_block_processor_append
 * repeatedly to add data to the file. Finally
 * call @ref sqfs_block_processor_end_file when you
 * are done. After writing all files, use @ref sqfs_block_processor_finish to
 * wait until all blocks that are still in flight are done and written to disk.
 *
 * The specified inode pointer is kept internally and updated with the
 * compressed block sizes and final destinations of the file and possible
 * fragment. You need to make sure it has enough backing-store for all blocks
 * to come. Furthermore, since there can still be blocks in-flight even after
 * calling @ref sqfs_block_processor_end_file, the data in the inode may still
 * change. The only point at which the data writer is guarnteed to not touch
 * them anymore is after @ref sqfs_block_processor_finish has returned.
 *
 * @param proc A pointer to a data writer object.
 * @param inode The regular file inode representing the file. The data writer
 *              internally updates it while writing blocks to disk.
 * @param flags A combination of @ref E_SQFS_BLK_FLAGS that can be used to
 *              micro manage how the data is processed.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_begin_file(sqfs_block_processor_t *proc,
					     sqfs_inode_generic_t *inode,
					     sqfs_u32 flags);

/**
 * @brief Append data to the current file.
 *
 * @memberof sqfs_block_processor_t
 *
 * Call this after @ref sqfs_block_processor_begin_file to add data to a file.
 *
 * @param proc A pointer to a data writer object.
 * @param data A pointer to a buffer to read data from.
 * @param size How many bytes should be copied out of the given
 *             buffer and written to disk.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_append(sqfs_block_processor_t *proc,
					 const void *data, size_t size);

/**
 * @brief Stop writing the current file and flush everything that is
 *        buffered internally.
 *
 * @memberof sqfs_block_processor_t
 *
 * The counter part to @ref sqfs_block_processor_begin_file.
 *
 * Even after calling this, there might still be data blocks in-flight.
 * Use @ref sqfs_block_processor_finish when you are done writing files to force
 * the remaining blocks to be processed and written to disk.
 *
 * @param proc A pointer to a data writer object.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_block_processor_end_file(sqfs_block_processor_t *proc);

/**
 * @brief Wait for the in-flight data blocks to finish and finally flush the
 *        current fragment block.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param proc A pointer to a block processor object.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR value on failure. The failure
 *         return value can either be an error encountered during enqueueing,
 *         processing or writing to disk.
 */
SQFS_API int sqfs_block_processor_finish(sqfs_block_processor_t *proc);

/**
 * @brief Register a set of hooks to be invoked when writing blocks to disk.
 *
 * @memberof sqfs_block_processor_t
 *
 * @param proc A pointer to a data writer object.
 * @param user_ptr A user pointer to pass to the callbacks.
 * @param hooks A structure containing the hooks.
 *
 * @return Zero on success, @ref SQFS_ERROR_UNSUPPORTED if the size field of
 *         the hooks doesn't match any size knwon to the library.
 */
SQFS_API
int sqfs_block_processor_set_hooks(sqfs_block_processor_t *proc, void *user_ptr,
				   const sqfs_block_hooks_t *hooks);

#ifdef __cplusplus
}
#endif

#endif /* SFQS_BLOCK_PROCESSOR_H */