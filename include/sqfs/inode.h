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
	sqfs_u16 type;

	/**
	 * @brief Mode filed holding permission bits only. The type is derived
	 *        from the type field.
	 */
	sqfs_u16 mode;

	/**
	 * @brief An index into the ID table where the owner UID is located.
	 */
	sqfs_u16 uid_idx;

	/**
	 * @brief An index into the ID table where the owner GID is located.
	 */
	sqfs_u16 gid_idx;

	/**
	 * @brief Last modifcation time.
	 *
	 * This field counts seconds (not counting leap seconds) since 00:00,
	 * Jan 1 1970 UTC. This field is unsigned, so it expires in the year
	 * 2106 (as opposed to 2038).
	 */
	sqfs_u32 mod_time;

	/**
	 * @brief Unique inode number
	 */
	sqfs_u32 inode_number;
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
	sqfs_u32 nlink;

	/**
	 * @brief Device number.
	 */
	sqfs_u32 devno;
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
	sqfs_u32 nlink;

	/**
	 * @brief Device number.
	 */
	sqfs_u32 devno;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
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
	sqfs_u32 nlink;
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
	sqfs_u32 nlink;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
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
	sqfs_u32 nlink;

	/**
	 * @brief Size of the symlink target in bytes
	 */
	sqfs_u32 target_size;

	/*sqfs_u8 target[];*/
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
	sqfs_u32 nlink;

	/**
	 * @brief Size of the symlink target in bytes
	 */
	sqfs_u32 target_size;

	/*sqfs_u8 target[];*/

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
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
	sqfs_u32 blocks_start;

	/**
	 * @brief Index into the fragment table or 0xFFFFFFFF if unused.
	 */
	sqfs_u32 fragment_index;

	/**
	 * @brief Offset into the uncompressed fragment block or 0xFFFFFFFF
	 *        if unused.
	 */
	sqfs_u32 fragment_offset;

	/**
	 * @brief Total, uncompressed size of the file in bytes.
	 */
	sqfs_u32 file_size;

	/*sqfs_u32 block_sizes[];*/
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
	sqfs_u64 blocks_start;

	/**
	 * @brief Total, uncompressed size of the file in bytes.
	 */
	sqfs_u64 file_size;

	/**
	 * @brief If the file is sparse, holds the number of bytes not written
	 *        to disk because of the omitted sparse blocks.
	 */
	sqfs_u64 sparse;

	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Index into the fragment table or 0xFFFFFFFF if unused.
	 */
	sqfs_u32 fragment_idx;

	/**
	 * @brief Offset into the uncompressed fragment block or 0xFFFFFFFF
	 *        if unused.
	 */
	sqfs_u32 fragment_offset;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;

	/*sqfs_u32 block_sizes[];*/
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
	sqfs_u32 start_block;

	/**
	 * @brief Number of hard links to this node.
	 */
	sqfs_u32 nlink;

	/**
	 * @brief Combined size of all directory entries and headers in bytes.
	 */
	sqfs_u16 size;

	/**
	 * @brief Offset into the uncompressed start block where the header can
	 *        be found.
	 */
	sqfs_u16 offset;

	/**
	 * @brief Inode number of the parent directory containing
	 *        this directory inode.
	 */
	sqfs_u32 parent_inode;
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
	sqfs_u32 nlink;

	/**
	 * @brief Combined size of all directory entries and headers in bytes.
	 */
	sqfs_u32 size;

	/**
	 * @brief Offset from the directory table start to the location of the
	 *        meta data block containing the first directory header.
	 */
	sqfs_u32 start_block;

	/**
	 * @brief Inode number of the parent directory containing
	 *        this directory inode.
	 */
	sqfs_u32 parent_inode;

	/**
	 * @brief Number of directory index entries following the inode
	 *
	 * This number is stored off-by one and counts the number of
	 * @ref sqfs_dir_index_t entries following the inode.
	 */
	sqfs_u16 inodex_count;

	/**
	 * @brief Offset into the uncompressed start block where the header can
	 *        be found.
	 */
	sqfs_u16 offset;

	/**
	 * @brief Extended attribute index.
	 */
	sqfs_u32 xattr_idx;
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
	sqfs_u32 *block_sizes;

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
	sqfs_u8 extra[];
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Get the extended attribute index of an inode
 *
 * For basic inodes, this returns the inode index 0xFFFFFFFF, i.e. the
 * sentinel value indicating that there are no xattrs.
 *
 * @param inode A pointer to an inode.
 * @param out Returns the extended attribute index on success.
 *
 * @return Zero on success, an @ref SQFS_ERROR_CORRUPTED if the node has
 *         an unknown type set.
 */
SQFS_API int sqfs_inode_get_xattr_index(const sqfs_inode_generic_t *inode,
					sqfs_u32 *out);

/**
 * @brief Convert a basic inode to an extended inode.
 *
 * For inodes that already have an extended type, this is a no-op.
 *
 * @param inode A pointer to an inode.
 *
 * @return Zero on success, an @ref SQFS_ERROR_CORRUPTED if the node has
 *         an unknown type set.
 */
SQFS_API int sqfs_inode_make_extended(sqfs_inode_generic_t *inode);

/**
 * @brief Convert an extended inode to a basic inode if possible.
 *
 * For inodes that already have a basic type, this is a no-op. If the inode
 * has values set that the coresponding basic type doesn't support (e.g. it
 * has an xattr index set or a regular file which requires 64 bit size
 * counter), it is left as an extended type and success state is returned.
 *
 * @param inode A pointer to an inode.
 *
 * @return Zero on success, an @ref SQFS_ERROR_CORRUPTED if the node has
 *         an unknown type set.
 */
SQFS_API int sqfs_inode_make_basic(sqfs_inode_generic_t *inode);

/**
 * @brief Update the file size of a regular file inode.
 *
 * If the new size is wider than 32 bit, a basic file inode is transparently
 * promoted to an extended file inode. For extended inodes, if the new size
 * is small enough and was the only requirement for the extended type, the
 * node is transparently demoted to a basic file inode.
 *
 * @param inode A pointer to an inode.
 * @param size The new size to set.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_set_file_size(sqfs_inode_generic_t *inode,
				      sqfs_u64 size);

/**
 * @brief Update the location of the first data block of a regular file inode.
 *
 * If the new location is wider than 32 bit, a basic file inode is
 * transparently promoted to an extended file inode. For extended inodes,
 * if the new size is small enough and was the only requirement for the
 * extended type, the node is transparently demoted to a basic file inode.
 *
 * @param inode A pointer to an inode.
 * @param location The new location to set.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_set_file_block_start(sqfs_inode_generic_t *inode,
					     sqfs_u64 location);

/**
 * @brief Update the file fragment location of a regular file inode.
 *
 * @param inode A pointer to an inode.
 * @param index The new fragment index to set.
 * @param offset The new fragment offset to set.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_set_frag_location(sqfs_inode_generic_t *inode,
					  sqfs_u32 index, sqfs_u32 offset);

/**
 * @brief Get the file size of a regular file inode.
 *
 * @param inode A pointer to an inode.
 * @param size Returns the file size.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_get_file_size(const sqfs_inode_generic_t *inode,
				      sqfs_u64 *size);

/**
 * @brief Get the file fragment location of a regular file inode.
 *
 * @param inode A pointer to an inode.
 * @param index Returns the fragment index.
 * @param offset Returns the fragment offset.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_get_frag_location(const sqfs_inode_generic_t *inode,
					  sqfs_u32 *index, sqfs_u32 *offset);

/**
 * @brief Get the location of the first data block of a regular file inode.
 *
 * @param inode A pointer to an inode.
 * @param location Returns the location.
 *
 * @return Zero on success, @ref SQFS_ERROR_NOT_FILE if the node is
 *         not a regular file.
 */
SQFS_API int sqfs_inode_get_file_block_start(const sqfs_inode_generic_t *inode,
					     sqfs_u64 *location);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_INODE_H */
