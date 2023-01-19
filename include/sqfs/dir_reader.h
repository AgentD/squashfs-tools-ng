/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_reader.h - This file is part of libsquashfs
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
#ifndef SQFS_DIR_READER_H
#define SQFS_DIR_READER_H

#include "sqfs/predef.h"

/**
 * @file dir_reader.h
 *
 * @brief Contains declarations for the @ref sqfs_dir_reader_t
 */

/**
 * @struct sqfs_dir_reader_t
 *
 * @implements sqfs_object_t
 *
 * @brief Abstracts reading of directory entries
 *
 * SquashFS stores directory listings and inode structures separated from
 * each other in meta data blocks.
 *
 * The sqfs_dir_reader_t abstracts access to the filesystem tree in a SquashFS
 * through a fairly simple interface. It keeps two meta data readers internally
 * for reading directory listings and inodes. Externally, it offers a few
 * simple functions for iterating over the contents of a directory that
 * completely take care of fetching/decoding headers and sifting through the
 * multi level hierarchie used for storing them on disk.
 *
 * See @ref sqfs_dir_writer_t for an overview on how directory entries are
 * stored in SquashFS.
 *
 * The reader also abstracts easy access to the underlying inodes, allowing
 * direct access to the inode referred to by a directory entry.
 */

/**
 * @enum SQFS_TREE_FILTER_FLAGS
 *
 * @brief Filter flags for @ref sqfs_dir_reader_get_full_hierarchy
 */
typedef enum {
	/**
	 * @brief Omit device special files from the final tree.
	 */
	SQFS_TREE_NO_DEVICES = 0x01,

	/**
	 * @brief Omit socket files from the final tree.
	 */
	SQFS_TREE_NO_SOCKETS = 0x02,

	/**
	 * @brief Omit named pipes from the final tree.
	 */
	SQFS_TREE_NO_FIFO = 0x04,

	/**
	 * @brief Omit symbolic links from the final tree.
	 */
	SQFS_TREE_NO_SLINKS = 0x08,

	/**
	 * @brief Omit empty directories from the final tree.
	 *
	 * If a directory is not empty on-disk, but ends up empty after
	 * applying all the other filter rules, it is also omitted.
	 */
	SQFS_TREE_NO_EMPTY = 0x10,

	/**
	 * @brief Do not recurse into sub directories.
	 *
	 * If the start node is a directory, the tree deserializer will still
	 * recurse into it, but it will not go beyond that.
	 */
	SQFS_TREE_NO_RECURSE = 0x20,

	/**
	 * @brief Store the list of parent nodes all the way to the target node
	 *
	 * When traversing towards the selected node, also collect the chain
	 * of parent nodes with the subtree stored at the end.
	 */
	SQFS_TREE_STORE_PARENTS = 0x40,

	SQFS_TREE_ALL_FLAGS = 0x7F,
} SQFS_TREE_FILTER_FLAGS;

/**
 * @struct sqfs_tree_node_t
 *
 * @brief Encapsulates a node in the filesystem tree read by
 *        @ref sqfs_dir_reader_get_full_hierarchy.
 */
struct sqfs_tree_node_t {
	/**
	 * @brief Pointer to parent, NULL for the root node
	 */
	sqfs_tree_node_t *parent;

	/**
	 * @brief For directories, a linked list of children.
	 */
	sqfs_tree_node_t *children;

	/**
	 * @brief Linked list next pointer for children list.
	 */
	sqfs_tree_node_t *next;

	/**
	 * @brief Inode representing this element in the tree.
	 */
	sqfs_inode_generic_t *inode;

	/**
	 * @brief Resolved 32 bit user ID from the inode
	 */
	sqfs_u32 uid;

	/**
	 * @brief Resolved 32 bit group ID from the inode
	 */
	sqfs_u32 gid;

	/**
	 * @brief null-terminated entry name.
	 */
	sqfs_u8 name[];
};

/**
 * @enum SQFS_DIR_READER_FLAGS
 *
 * @brief Flags for @ref sqfs_dir_reader_create
 */
typedef enum {
	/**
	 * @brief Support "." and ".." directory and path entries.
	 *
	 * If this flag is set, the directory reader returns "." and ".."
	 * entries when iterating over a directory, can fetch the associated
	 * inodes if requested and supports resolving "." and ".." path
	 * components when looking up a full path.
	 *
	 * In order for this to work, it internally caches the locations of
	 * directory inodes it encounteres. This means, it only works as long
	 * as you only use inodes fetched through the directory reader. If
	 * given a foreign inode it hasn't seen before, it might not be able
	 * to resolve the parent link.
	 */
	SQFS_DIR_READER_DOT_ENTRIES = 0x00000001,

	SQFS_DIR_READER_ALL_FLAGS = 0x00000001,
} SQFS_DIR_READER_FLAGS;

/**
 * @enum SQFS_DIR_OPEN_FLAGS
 *
 * @brief Flags for @ref sqfs_dir_reader_open_dir
 */
typedef enum {
	/**
	 * @brief Do not generate "." and ".." entries
	 *
	 * If the @ref sqfs_dir_reader_t was created with
	 * the @ref SQFS_DIR_READER_DOT_ENTRIES flag set, "." and ".." entries
	 * are generated when iterating over a directory. If that is not desired
	 * in some instances, this flag can be set to suppress this behaviour
	 * when opening a directory.
	 */
	SQFS_DIR_OPEN_NO_DOT_ENTRIES = 0x00000001,

	SQFS_DIR_OPEN_ALL_FLAGS = 0x00000001,
} SQFS_DIR_OPEN_FLAGS;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create a directory reader.
 *
 * @memberof sqfs_dir_reader_t
 *
 * The function fails if any unknown flag is set. In squashfs-tools-ng
 * version 1.2 introduced the @ref SQFS_DIR_READER_DOT_ENTRIES flag,
 * earlier versions require the flags field to be set to zero.
 *
 * @param super A pointer to the super block.
 * @param cmp A compressor to use for unpacking meta data blocks.
 * @param file The input file to read from.
 * @param flags A combination of @ref SQFS_DIR_READER_FLAGS
 *
 * @return A new directory reader on success, NULL on allocation failure.
 */
SQFS_API sqfs_dir_reader_t *sqfs_dir_reader_create(const sqfs_super_t *super,
						   sqfs_compressor_t *cmp,
						   sqfs_file_t *file,
						   sqfs_u32 flags);

/**
 * @brief Navigate a directory reader to the location of a directory
 *        represented by an inode.
 *
 * @memberof sqfs_dir_reader_t
 *
 * This function seeks to the meta data block containing the directory
 * listing that the given inode referes to and resets the internal state.
 * After that, consequtive cals to @ref sqfs_dir_reader_read can be made
 * to iterate over the directory contents.
 *
 * If the reader was created with the @ref SQFS_DIR_READER_DOT_ENTRIES flag
 * set, the first two entries will be ".", referring to the directory inode
 * itself and "..", referring to the parent directory inode. Those entries
 * are generated artificially, as SquashFS does not store them on disk, hence
 * extra work is required and a flag is used to enable this behaviour. By
 * default, no such entries are generated.
 *
 * If this flag is set, but you wish to override that behaviour on a
 * per-instance basis, simply set the @ref SQFS_DIR_OPEN_NO_DOT_ENTRIES flag
 * when calling this function.
 *
 * @param rd A pointer to a directory reader.
 * @param inode An directory or extended directory inode.
 * @param flags A combination of @ref SQFS_DIR_OPEN_FLAGS.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_open_dir(sqfs_dir_reader_t *rd,
				      const sqfs_inode_generic_t *inode,
				      sqfs_u32 flags);

/**
 * @brief Reset a directory reader back to the beginning of the listing.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_rewind(sqfs_dir_reader_t *rd);

/**
 * @brief Seek through the current directory listing to locate an
 *        entry by name.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param name The name of the entry to find.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_find(sqfs_dir_reader_t *rd, const char *name);

/**
 * @brief Read a directory entry and advance the internal position indicator
 *        to the next one.
 *
 * @memberof sqfs_dir_reader_t
 *
 * Call this function repeatedly to iterate over a directory listing. It
 * returns a positive number to indicate that it couldn't fetch any more data
 * because the end of the listing was reached. A negative value indicates an
 * error.
 *
 * After calling this function, you can use @ref sqfs_dir_reader_get_inode to
 * read the full inode structure that the current entry referes to.
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a directory entry on success that can be
 *            freed with a single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure, a positive
 *         number if the end of the current directory listing has been reached.
 */
SQFS_API int sqfs_dir_reader_read(sqfs_dir_reader_t *rd,
				  sqfs_dir_entry_t **out);

/**
 * @brief Read the inode that the current directory entry points to.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_get_inode(sqfs_dir_reader_t *rd,
				       sqfs_inode_generic_t **inode);

/**
 * @brief Read the root inode using the location given by the super block.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_get_root_inode(sqfs_dir_reader_t *rd,
					    sqfs_inode_generic_t **inode);

/**
 * @brief Find an inode through path traversal starting from the root or a
 *        given node downwards.
 *
 * @memberof sqfs_dir_reader_t
 *
 * @param rd A pointer to a directory reader.
 * @param start If not NULL, path traversal starts at this node downwards. If
 *              set to NULL, start at the root node.
 * @param path A path to resolve into an inode. Forward or backward slashes can
 *             be used to separate path components. Resolving '.' or '..' is
 *             not supported.
 * @param out Returns a pointer to a generic inode that can be freed with a
 *            single @ref sqfs_free call.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_find_by_path(sqfs_dir_reader_t *rd,
					  const sqfs_inode_generic_t *start,
					  const char *path,
					  sqfs_inode_generic_t **out);

/**
 * @brief High level helper function for deserializing the entire file system
 *        hierarchy into an in-memory tree structure.
 *
 * @memberof sqfs_dir_reader_t
 *
 * This function internally navigates to a specified inode using
 * @ref sqfs_dir_reader_find_by_path and starting from that recursively
 * deserializes the entire hierarchy into a tree structure holding all inodes.
 *
 * @param rd A pointer to a directory reader.
 * @param path A path to resolve into an inode. Forward or backward slashes can
 *             be used to separate path components. Resolving '.' or '..' is
 *             not supported. Can be set to NULL to get the root inode.
 * @param flags A combination of @ref SQFS_TREE_FILTER_FLAGS flags.
 * @param out Returns the top most tree node.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_dir_reader_get_full_hierarchy(sqfs_dir_reader_t *rd,
						const sqfs_id_table_t *idtbl,
						const char *path,
						sqfs_u32 flags,
						sqfs_tree_node_t **out);

/**
 * @brief Recursively destroy a tree of @ref sqfs_tree_node_t nodes
 *
 * This function can be used to clean up after
 * @ref sqfs_dir_reader_get_full_hierarchy.
 *
 * @param root A pointer to the root node or NULL.
 */
SQFS_API void sqfs_dir_tree_destroy(sqfs_tree_node_t *root);

/**
 * @brief Recursively destroy a tree of @ref sqfs_tree_node_t nodes
 *
 * @memberof sqfs_tree_node_t
 *
 * This function can be used to assemble an absolute path from a tree
 * node returned by @ref sqfs_dir_reader_get_full_hierarchy.
 *
 * The function recursively walks up the tree to assemble a path string. It
 * returns "/" for the root node and assembles paths beginning with "/" for
 * non-root nodes. The resulting path is slash separated, but (except for
 * the root) never ends with a slash.
 *
 * While walking the node list, the function enforces various invariantes. It
 * returns @ref SQFS_ERROR_LINK_LOOP if the list of parent pointers is cyclical,
 * @ref SQFS_ERROR_CORRUPTED if any node has an empty name, or a name that
 * contains '/' or equals ".." or ".". The function
 * returns @ref SQFS_ERROR_ARG_INVALID if given NULL node or the root has a name
 * set. Additionally, the function can return overflow or allocation failures
 * while constructing the path.
 *
 * The returned string needs to be free'd with @ref sqfs_free.
 *
 * @param node A pointer to a tree node.
 * @param out Returns a pointer to a string on success, set to NULL on failure.
 *
 * @return Zero on success, an @ref SQFS_ERROR value on failure.
 */
SQFS_API int sqfs_tree_node_get_path(const sqfs_tree_node_t *node,
				     char **out);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_DIR_READER_H */
