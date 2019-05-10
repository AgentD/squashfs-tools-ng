/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FSTREE_H
#define FSTREE_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "str_table.h"

#define FSTREE_XATTR_KEY_BUCKETS 31
#define FSTREE_XATTR_VALUE_BUCKETS 511

typedef struct tree_node_t tree_node_t;
typedef struct file_info_t file_info_t;
typedef struct dir_info_t dir_info_t;
typedef struct fstree_t fstree_t;
typedef struct tree_xattr_t tree_xattr_t;

/**
 * @struct tree_xattr_t
 *
 * @brief Encapsulates a set of key-value pairs attached to a @ref tree_node_t
 */
struct tree_xattr_t {
	size_t num_attr;
	size_t max_attr;

	/**
	 * @brief Back reference to the tree node this was created for
	 */
	tree_node_t *owner;

	/**
	 * @brief linked list pointer of list of attributes in @ref fstree_t
	 */
	tree_xattr_t *next;

	/**
	 * @brief An array with pairs of key-value tupples
	 *
	 * Each key-value tupple is encoded as (key << 32) | value.
	 */
	uint64_t ref[];
};

/**
 * @struct file_info_t
 *
 * @brief Additional meta data stored in a @ref tree_node_t for regular files
 */
struct file_info_t {
	char *input_file;

	/**
	 * @brief Linked list pointer for aggregating fragments
	 *
	 * When writing out data blocks, files that don't have a multiple of
	 * the block size have their tail ends gathered in a fragment block.
	 * A linked list is used to keep track of which files share the same
	 * fragment block.
	 */
	file_info_t *frag_next;

	/**
	 * @brief Total size of the file in bytes
	 */
	uint64_t size;

	/**
	 * @brief Absolute position of the first data block
	 */
	uint64_t startblock;

	/**
	 * @brief Fragment index
	 *
	 * If the size is not a multiple of the block size, this holds an
	 * index into the fragment table.
	 */
	uint32_t fragment;

	/**
	 * @brief Byte offset into the fragment block
	 *
	 * If the size is not a multiple of the block size, this holds an
	 * offset into the fragment block.
	 */
	uint32_t fragment_offset;

	/**
	 * @brief Stores the compressed file block sizes
	 *
	 * For each full data block, stores the compressed size. Bit number
	 * 24 is set if the block is stored uncompressed.
	 */
	uint32_t blocksizes[];
};

/**
 * @struct dir_info_t
 *
 * @brief Additional meta data stored in a @ref tree_node_t for directories
 */
struct dir_info_t {
	/**
	 * @brief Pointer to the head of the linked list of children
	 */
	tree_node_t *children;

	/**
	 * @brief Size of the directory
	 *
	 * Computed and updated on the fly while writing directory
	 * meta data to disk.
	 */
	uint64_t size;

	/**
	 * @brief Start block offset, relative to directory table start
	 *
	 * Offset of the compressed meta data block where the directory
	 * listing is stored in the SquashFS image.
	 */
	uint64_t start_block;

	/**
	 * @brief Byte offset into the uncompressed meta data block
	 *
	 * Points at where in the meta data block the directory listing begins.
	 */
	uint32_t block_offset;

	/**
	 * @brief Set to true for implicitly generated directories
	 */
	bool created_implicitly;
};

/**
 * @struct tree_node_t
 *
 * @brief A node in a file system tree
 */
struct tree_node_t {
	/**
	 * @brief Linked list pointer to the next node in the same directory
	 */
	tree_node_t *next;

	/**
	 * @brief Pointer to directory node that this node is in
	 *
	 * This is NULL only for the root node
	 */
	tree_node_t *parent;

	/**
	 * @brief Pointer into the payload area where the node name is stored
	 *
	 * For the root node, this points to an empty string
	 */
	char *name;

	/**
	 * @brief A pointer to an extended attribute array or NULL if unused
	 */
	tree_xattr_t *xattr;

	uint32_t uid;
	uint32_t gid;
	uint16_t mode;

	/**
	 * @brief SquashFS inode refernce number
	 *
	 * This is computed and stored here on the fly when writing inodes
	 * generated from tree nodes to the SquashFS image.
	 *
	 * An inode reference is the 32 bit offset of the compressed meta data
	 * block, shifted left by 16 and ored with a 13 bit offset into the
	 * uncompressed meta data block.
	 */
	uint64_t inode_ref;

	/**
	 * @brief inode number
	 *
	 * This is computed and stored here on the fly when writing inodes
	 * generated from tree nodes to the SquashFS image.
	 */
	uint32_t inode_num;

	/**
	 * @brief SquashFS inode type used for this tree node
	 *
	 * This is computed and stored here on the fly when writing inodes
	 * generated from tree nodes to the SquashFS image. It can't be
	 * easily determined in advance since it depends also on the size
	 * of the node, which means for directories the size of the directory
	 * entries once written to disk.
	 *
	 * All code that actually processes tree nodes should use the mode
	 * field instead (mode & S_IFMT gives us the node type). It is stored
	 * here when generating inodes since we need it later on to generate
	 * directory entries.
	 */
	int type;

	union {
		/**
		 * @brief Pointer into payload area storing directory meta data
		 */
		dir_info_t *dir;

		/**
		 * @brief Pointer into payload area storing file meta data
		 */
		file_info_t *file;

		/**
		 * @brief Pointer into payload area storing symlink target
		 */
		char *slink_target;

		/**
		 * @brief A device number for device special files
		 */
		uint64_t devno;
	} data;

	/**
	 * @brief Additional data stored in the tree node
	 */
	uint8_t payload[];
};

/**
 * @struct fstree_t
 *
 * @brief Encapsulates a file system tree
 */
struct fstree_t {
	uint32_t default_uid;
	uint32_t default_gid;
	uint32_t default_mode;
	uint32_t default_mtime;
	size_t block_size;

	str_table_t xattr_keys;
	str_table_t xattr_values;

	tree_node_t *root;
	tree_xattr_t *xattr;
};

/**
 * @brief Initialize an fstree object
 *
 * @memberof fstree_t
 *
 * Initializing means copying over the default values and creating a root node.
 * On error, an error message is written to stderr.
 *
 * @param fs           A pointer to an uninitialized fstree object
 * @param block_size   The data block size for regular files
 * @param mtime        Default modification time stamp to use on all nodes
 * @param default_mode Default permission bits to use on implicitly created
 *                     directories.
 * @param default_uid  Default UID to set on implicitly created directories.
 * @param default_gid  Default GID to set on implicitly created directories.
 *
 * @return Zero on success, -1 on failure
 */
int fstree_init(fstree_t *fs, size_t block_size, uint32_t mtime,
		uint16_t default_mode, uint32_t default_uid,
		uint32_t default_gid);

/**
 * @brief Clean up an fstree object and free all memory it uses
 *
 * @memberof fstree_t
 *
 * This function also recursively frees all tree nodes.
 *
 * @param fs A pointer to an fstree object
 */
void fstree_cleanup(fstree_t *fs);

/**
 * @brief Add a generic node to an fstree object
 *
 * @memberof fstree_t
 *
 * The new node is inserted by path. If some components of the path don't
 * exist, they are created as directories with default permissions, like
 * mkdir -p would, and marked as implcitily created. A subsequent call that
 * tries to create an existing tree node will fail, except if the target
 * is an implicitly created directory node and the call tries to create it
 * as a directory. This will simply overwrite the permissions and ownership.
 * The implicitly created flag is then cleared and subsequent attempts to
 * create this directory again will also fail.
 *
 * This function does not print anything to stderr, instead it sets an
 * appropriate errno value.
 *
 * @param fs        A pointer to an fstree object
 * @param path      The path of the new object to insert
 * @param mode      Specifies both access permission and what kind of node
 *                  to create
 * @param uid       The user id that owns the new node
 * @param gid       The group id that owns the new node
 * @param extra_len An additional number of bytes to allocate for payload data
 *
 * @return A pointer to the new tree node or NULL on failure
 */
tree_node_t *fstree_add(fstree_t *fs, const char *path, uint16_t mode,
			uint32_t uid, uint32_t gid, size_t extra_len);

/**
 * @brief A wrappter around @ref fstree_add for regular files
 *
 * @memberof fstree_t
 *
 * This function internally computes the number of extra payload bytes
 * requiered and sets up the payload pointers propperly, as they are
 * different than for other types of nodes.
 *
 * @return A pointer to the new tree node or NULL on failure
 */
tree_node_t *fstree_add_file(fstree_t *fs, const char *path, uint16_t mode,
			     uint32_t uid, uint32_t gid, uint64_t filesz,
			     const char *input);

/**
 * @brief Add an extended attribute key value pair to a tree node
 *
 * @memberof fstree_t
 *
 * @param fs    A pointer to the fstree object
 * @param node  A pointer to the tree node to attach attributes to
 * @param key   The xattr key with namespace prefix
 * @param value The xattr value string
 *
 * @return Zero on success, -1 on failure (prints error to stderr)
 */
int fstree_add_xattr(fstree_t *fs, tree_node_t *node,
		     const char *key, const char *value);

/**
 * @brief Remove dupliciate xattr listings
 *
 * @memberof fstree_t
 *
 * If two tree nodes have pointers to distinct @ref tree_xattr_t listings that
 * turn out to be equivalent, throw one of the two away and make both nodes
 * point to the same instance.
 */
void fstree_xattr_deduplicate(fstree_t *fs);

/**
 * @brief Load an fstree from a text file describing it
 *
 * @memberof fstree_t
 *
 * This function parses the file format accepted by gensquashfs and restores
 * a file system tree from it.
 *
 * On failure, an error report with filename and line number is written
 * to stderr.
 *
 * @param fs       A pointer to an fstree object that is already initialized
 *                 prior to calling this function.
 * @param filename The path to the input file to process.
 *
 * @return Zero on success, -1 on failure.
 */
int fstree_from_file(fstree_t *fs, const char *filename);

/**
 * @brief Lexicographically sort all directory contents
 *
 * @memberof fstree_t
 *
 * @param fs A pointer to an fstree object
 */
void fstree_sort(fstree_t *fs);

#endif /* FSTREE_H */
