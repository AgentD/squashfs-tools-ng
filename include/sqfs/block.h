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
 * @brief Contains on-disk data structures for data block management,
 *        helper macros and the higher level @ref sqfs_block_t structure.
 */

#define SQFS_META_BLOCK_SIZE 8192

#define SQFS_IS_BLOCK_COMPRESSED(size) (((size) & (1 << 24)) == 0)
#define SQFS_ON_DISK_BLOCK_SIZE(size) ((size) & ((1 << 24) - 1))
#define SQFS_IS_SPARSE_BLOCK(size) (SQFS_ON_DISK_BLOCK_SIZE(size) == 0)

/**
 * @struct sqfs_fragment_t
 *
 * @brief Data structure that makes up the fragment table entries.
 */
struct sqfs_fragment_t {
	/**
	 * @brief Location of the fragment block on-disk.
	 */
	sqfs_u64 start_offset;

	/**
	 * @brief Size of the fragment block in bytes.
	 */
	sqfs_u32 size;

	/**
	 * @brief Unused. Always initialize this to 0.
	 */
	sqfs_u32 pad0;
};

/**
 * @enum E_SQFS_BLK_FLAGS
 *
 * @brief Generic flags that tell the processor what to do with a block and
 *        flags that the processor sets when it is done with a block.
 */
typedef enum {
	/**
	 * @brief Only calculate checksum, do NOT compress the data.
	 *
	 * If set, the blocks of a file will not be compressed by the
	 * @ref sqfs_data_writer_t.
	 */
	SQFS_BLK_DONT_COMPRESS = 0x0001,

	/**
	 * @brief Allign the block on disk to device block size.
	 *
	 * If set, the @ref sqfs_data_writer_t will add padding before the
	 * first block of the affected file and after the last block.
	 */
	SQFS_BLK_ALLIGN = 0x0002,

	/**
	 * @brief Set by the @ref sqfs_data_writer_t on the first
	 *        block of a file.
	 */
	SQFS_BLK_FIRST_BLOCK = 0x0800,

	/**
	 * @brief Set by the @ref sqfs_data_writer_t on the last
	 *        block of a file.
	 */
	SQFS_BLK_LAST_BLOCK = 0x1000,

	/**
	 * @brief Set by the @ref sqfs_data_writer_t to indicate that a block
	 *        is a tail end of a file and the block.
	 */
	SQFS_BLK_IS_FRAGMENT = 0x2000,

	/**
	 * @brief Set by the @ref sqfs_data_writer_t on fragment blocks that
	 *        it generates.
	 */
	SQFS_BLK_FRAGMENT_BLOCK = 0x4000,

	/**
	 * @brief Set by @ref sqfs_data_writer_t if the block was
	 *        actually compressed.
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
	sqfs_u32 sequence_number;

	/**
	 * @brief Size of the data area.
	 */
	sqfs_u32 size;

	/**
	 * @brief Checksum of the input data.
	 */
	sqfs_u32 checksum;

	/**
	 * @brief Data block index within the inode.
	 */
	sqfs_u32 index;

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
	sqfs_u32 flags;

	/**
	 * @brief Raw data to be processed.
	 */
	sqfs_u8 data[];
};

#endif /* SQFS_BLOCK_H */
