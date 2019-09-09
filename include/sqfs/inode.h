/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * inode.h - This file is part of libsquashfs
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
#ifndef SQFS_INODE_H
#define SQFS_INODE_H

#include "sqfs/predef.h"

/**
 * @file inode.h
 *
 * @brief Contains on-disk data structures used for inodes.
 */

/**
 * @enum E_SQFS_INODE_TYPE
 *
 * @brief Used by @ref sqfs_inode_t to identify the inode type.
 */
typedef enum {
	SQFS_INODE_DIR = 1,
	SQFS_INODE_FILE = 2,
	SQFS_INODE_SLINK = 3,
	SQFS_INODE_BDEV = 4,
	SQFS_INODE_CDEV = 5,
	SQFS_INODE_FIFO = 6,
	SQFS_INODE_SOCKET = 7,
	SQFS_INODE_EXT_DIR = 8,
	SQFS_INODE_EXT_FILE = 9,
	SQFS_INODE_EXT_SLINK = 10,
	SQFS_INODE_EXT_BDEV = 11,
	SQFS_INODE_EXT_CDEV = 12,
	SQFS_INODE_EXT_FIFO = 13,
	SQFS_INODE_EXT_SOCKET = 14,
} E_SQFS_INODE_TYPE;

/**
 * @struct sqfs_inode_t
 *
 * @brief Common inode structure
 *
 * This structure holds the fields common for all inodes. Depending on the type
 * field, a specific inode structure follows.
 */
struct sqfs_inode_t {
	/**
	 * @brief An @ref E_SQFS_INODE_TYPE value.
	 */
	uint16_t type;

	/**
	 * @brief Mode filed holding permission bits only. The type is derived
	 *        from the type field.
	 */
	uint16_t mode;

	/**
	 * @brief An index into the ID table where the owner UID is located.
	 */
	uint16_t uid_idx;

	/**
	 * @brief An index into the ID table where the owner GID is located.
	 */
	uint16_t gid_idx;

	/**
	 * @brief Last modifcation time.
	 *
	 * This field counts seconds (not counting leap seconds) since 00:00,
	 * Jan 1 1970 UTC. This field is unsigned, so it expires in the year
	 * 2106 (as opposed to 2038).
	 */
	uint32_t mod_time;

	/**
	 * @brief Unique inode number
	 */
	uint32_t inode_number;
};

/**
 * @struct sqfs_inode_dev_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_BDEV
 *        or @ref SQFS_INODE_CDEV.
 */
struct sqfs_inode_dev_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Device number.
	 */
	uint32_t devno;
};

/**
 * @struct sqfs_inode_dev_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_BDEV
 *        or @ref SQFS_INODE_EXT_CDEV.
 */
struct sqfs_inode_dev_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Device number.
	 */
	uint32_t devno;

	/**
	 * @brief Extended attribute index.
	 */
	uint32_t xattr_idx;
};

/**
 * @struct sqfs_inode_ipc_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_FIFO
 *        or @ref SQFS_INODE_SOCKET.
 */
struct sqfs_inode_ipc_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;
};

/**
 * @struct sqfs_inode_ipc_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_FIFO
 *        or @ref SQFS_INODE_EXT_SOCKET.
 */
struct sqfs_inode_ipc_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Extended attribute index.
	 */
	uint32_t xattr_idx;
};

/**
 * @struct sqfs_inode_slink_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_SLINK.
 *
 * The declaration does not contain the flexible array member of the symlink
 * target because @ref sqfs_inode_generic_t would otherwies be impossible to
 * implement without violating the C standard.
 */
struct sqfs_inode_slink_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Size of the symlink target in bytes
	 */
	uint32_t target_size;

	/*uint8_t target[];*/
};

/**
 * @struct sqfs_inode_slink_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_SLINK.
 *
 * The declaration does not contain the flexible array member of the symlink
 * target because it is wedged right in between the target size and the xattr
 * identifier.
 */
struct sqfs_inode_slink_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Size of the symlink target in bytes
	 */
	uint32_t target_size;

	/*uint8_t target[];*/

	/**
	 * @brief Extended attribute index.
	 */
	uint32_t xattr_idx;
};

/**
 * @struct sqfs_inode_file_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_FILE.
 *
 * The declaration does not contain the flexible array member for the data
 * block sizes because @ref sqfs_inode_generic_t would otherwies be impossible
 * to implement without violating the C standard.
 *
 * For each data block, the inode is followed by a 32 bit integer that holds
 * the on-disk size of the compressed block in bytes and has bit number 24
 * set if the block is stored uncompressed.
 *
 * If a block size is specified as zero, it is assumed to be an entire block
 * filled with zero bytes.
 */
struct sqfs_inode_file_t {
	/**
	 * @brief Absolute position of the first data block.
	 */
	uint32_t blocks_start;

	/**
	 * @brief Index into the fragment table or 0xFFFFFFFF if unused.
	 */
	uint32_t fragment_index;

	/**
	 * @brief Offset into the uncompressed fragment block or 0xFFFFFFFF
	 *        if unused.
	 */
	uint32_t fragment_offset;

	/**
	 * @brief Total, uncompressed size of the file in bytes.
	 */
	uint32_t file_size;

	/*uint32_t block_sizes[];*/
};

/**
 * @struct sqfs_inode_file_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_FILE.
 *
 * @copydoc sqfs_inode_file_t
 */
struct sqfs_inode_file_ext_t {
	/**
	 * @brief Absolute position of the first data block.
	 */
	uint64_t blocks_start;

	/**
	 * @brief Total, uncompressed size of the file in bytes.
	 */
	uint64_t file_size;

	/**
	 * @brief If the file is sparse, holds the number of bytes not written
	 *        to disk because of the omitted sparse blocks.
	 */
	uint64_t sparse;

	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Index into the fragment table or 0xFFFFFFFF if unused.
	 */
	uint32_t fragment_idx;

	/**
	 * @brief Offset into the uncompressed fragment block or 0xFFFFFFFF
	 *        if unused.
	 */
	uint32_t fragment_offset;

	/**
	 * @brief Extended attribute index.
	 */
	uint32_t xattr_idx;

	/*uint32_t block_sizes[];*/
};

/**
 * @struct sqfs_inode_dir_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_DIR.
 */
struct sqfs_inode_dir_t {
	/**
	 * @brief Offset from the directory table start to the location of the
	 *        meta data block containing the first directory header.
	 */
	uint32_t start_block;

	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Combined size of all directory entries and headers in bytes.
	 */
	uint16_t size;

	/**
	 * @brief Offset into the uncompressed start block where the header can
	 *        be found.
	 */
	uint16_t offset;

	/**
	 * @brief Inode number of the parent directory containing
	 *        this directory inode.
	 */
	uint32_t parent_inode;
};

/**
 * @struct sqfs_inode_dir_ext_t
 *
 * @brief Follows a @ref sqfs_inode_t if type is @ref SQFS_INODE_EXT_DIR.
 */
struct sqfs_inode_dir_ext_t {
	/**
	 * @brief Number of hard links to this node.
	 */
	uint32_t nlink;

	/**
	 * @brief Combined size of all directory entries and headers in bytes.
	 */
	uint32_t size;

	/**
	 * @brief Offset from the directory table start to the location of the
	 *        meta data block containing the first directory header.
	 */
	uint32_t start_block;

	/**
	 * @brief Inode number of the parent directory containing
	 *        this directory inode.
	 */
	uint32_t parent_inode;

	/**
	 * @brief Number of directory index entries following the inode
	 *
	 * This number is stored off-by one and counts the number of
	 * @ref sqfs_dir_index_t entries following the inode.
	 */
	uint16_t inodex_count;

	/**
	 * @brief Offset into the uncompressed start block where the header can
	 *        be found.
	 */
	uint16_t offset;

	/**
	 * @brief Extended attribute index.
	 */
	uint32_t xattr_idx;
};

/**
 * @struct sqfs_inode_generic_t
 *
 * @brief A generic inode structure that combines all others and provides
 *        additional information.
 *
 * A few helper functions exist for working with this. For instance,
 * @ref sqfs_meta_reader_read_inode can read an inode from disk and assemble it
 * into an instance of this structure. Similarly, the
 * @ref sqfs_meta_writer_write_inode function can break it down into encoded,
 * on-disk structures and write them to disk.
 */
struct sqfs_inode_generic_t {
	/**
	 * @brief The common fields for all inodes.
	 */
	sqfs_inode_t base;

	/**
	 * @brief A pointer into the extra field holding the symlink target.
	 *
	 * @param This string is not null terminated. The helper functions rely
	 *        entirely on the length stored in the symlink inode.
	 */
	char *slink_target;

	/**
	 * @brief A pointer into the extra field holding file blocks sizes.
	 *
	 * For file inodes, holds the consecutive block sizes. Bit number 24 is
	 * set if the block is stored uncompressed. If it the size is zero,
	 * the block is sparse.
	 */
	uint32_t *block_sizes;

	/**
	 * @brief For file inodes, stores the number of blocks used.
	 */
	size_t num_file_blocks;

	/**
	 * @brief Type specific inode data.
	 */
	union {
		sqfs_inode_dev_t dev;
		sqfs_inode_dev_ext_t dev_ext;
		sqfs_inode_ipc_t ipc;
		sqfs_inode_ipc_ext_t ipc_ext;
		sqfs_inode_slink_t slink;
		sqfs_inode_slink_ext_t slink_ext;
		sqfs_inode_file_t file;
		sqfs_inode_file_ext_t file_ext;
		sqfs_inode_dir_t dir;
		sqfs_inode_dir_ext_t dir_ext;
	} data;

	/**
	 * @brief Holds type specific extra data, such as symlink target.
	 */
	uint8_t extra[];
};

#endif /* SQFS_INODE_H */
