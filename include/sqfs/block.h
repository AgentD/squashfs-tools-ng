/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block.h - This file is part of libsquashfs
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
#ifndef SQFS_BLOCK_H
#define SQFS_BLOCK_H

#include "sqfs/predef.h"

/**
 * @file block.h
 *
 * @brief Contains the definition of the @ref sqfs_block_t data structure.
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
	 * @brief Allign the block on disk to device block size.
	 *
	 * If set in combination with @ref SQFS_BLK_FIRST_BLOCK, the output
	 * file is padded to a multiple of the device block size before writing
	 * the block.
	 *
	 * If used with @ref SQFS_BLK_LAST_BLOCK, the output file is padded
	 * after writing the block.
	 */
	SQFS_BLK_ALLIGN = 0x0002,

	/**
	 * @brief Indicates that an equeued block is the first block of a file.
	 */
	SQFS_BLK_FIRST_BLOCK = 0x0800,

	/**
	 * @brief Indicates that an equeued block is the last block of a file.
	 */
	SQFS_BLK_LAST_BLOCK = 0x1000,

	/**
	 * @brief Indicates that a block is a tail end of a file and the block
	 *        processor should take care of fragment packing and accounting.
	 */
	SQFS_BLK_IS_FRAGMENT = 0x2000,

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
	SQFS_BLK_USER_SETTABLE_FLAGS = 0x0003,
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

#endif /* SQFS_BLOCK_H */
