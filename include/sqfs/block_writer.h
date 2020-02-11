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
	 * The function may write additional data to the file, which is taken
	 * into account when padding the file.
	 *
	 * @param user A user pointer.
	 * @param flags A pointer to a combination of @ref E_SQFS_BLK_FLAGS
	 *              describing the block. The callback can modify the
	 *              user settable flags.
	 * @param size The size of the block in bytes.
	 * @param data A pointer to the raw block data.
	 * @param file The file that the block will be written to.
	 */
	void (*pre_block_write)(void *user, sqfs_u32 *flags, sqfs_u32 size,
				const sqfs_u8 *data, sqfs_file_t *file);

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
	 * @param flags A combination of @ref E_SQFS_BLK_FLAGS describing
	 *              the block.
	 * @param size The size of the block in bytes.
	 * @param data A pointer to the raw block data.
	 * @param file The file that the block was written to.
	 */
	void (*post_block_write)(void *user, sqfs_u32 flags, sqfs_u32 size,
				 const sqfs_u8 *data, sqfs_file_t *file);

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
 * @brief Register a set of callbacks with a block writer.
 *
 * @memberof sqfs_block_writer_t
 *
 * @param wr A pointer to a block writer object.
 * @param user_ptr A user data pointer that should be passed to the callbacks.
 * @param hooks A structure holding various callbacks.
 *
 * @return Zero on success, an @ref E_SQFS_ERROR on failure.
 */
SQFS_API int sqfs_block_writer_set_hooks(sqfs_block_writer_t *wr,
					 void *user_ptr,
					 const sqfs_block_hooks_t *hooks);

/**
 * @brief Destroy a block writer object.
 *
 * @memberof sqfs_block_writer_t
 *
 * @param wr A pointer to a block writer object.
 */
SQFS_API void sqfs_block_writer_destroy(sqfs_block_writer_t *wr);

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
 * @return Zero on success, an @ref E_SQFS_ERROR error on failure.
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
