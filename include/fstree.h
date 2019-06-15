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

/* Encapsulates a set of key-value pairs attached to a tree_node_t */
struct tree_xattr_t {
	/* Number of key-value pairs */
	size_t num_attr;

	/* Total size of the array, i.e. it's capacity */
	size_t max_attr;

	/* Offset of the meta data block where the pairs are stored */
	uint64_t block;

	/* Offset into a meta data block where the pairs start */
	uint32_t offset;

	/* Number of bytes written to disk */
	uint32_t size;

	/* Incremental index within all xattr blocks */
	size_t index;

	/* Back reference to the tree node this was created for */
	tree_node_t *owner;

	/* linked list pointer of list of attributes in @ref fstree_t */
	tree_xattr_t *next;

	/* Array with pairs of key-value tupples.
	   Each key-value tupple is encoded as (key << 32) | value. */
	uint64_t ref[];
};

/* Additional meta data stored in a tree_node_t for regular files. */
struct file_info_t {
	/* Path to the input file. */
	char *input_file;

	uint64_t size;

	/* Absolute position of the first data block. */
	uint64_t startblock;

	/* If the size is not a multiple of the block size, this holds an
	   index into the fragment table. */
	uint32_t fragment;

	/* Byte offset into the fragment block. */
	uint32_t fragment_offset;

	/* For each full data block, stores the compressed size.
	   Bit (1 << 24) is set if the block is stored uncompressed. */
	uint32_t blocksizes[];
};

/* Additional meta data stored in a tree_node_t for directories */
struct dir_info_t {
	/* Linked list head for children in the directory */
	tree_node_t *children;

	/* Size on disk. Computed and updated on the fly while writing
	   directory meta data to disk. */
	uint64_t size;

	/* Start block offset, relative to directory table start. */
	uint64_t start_block;

	/* Byte offset into the uncompressed meta data block. */
	uint32_t block_offset;

	/* Set to true for implicitly generated directories.  */
	bool created_implicitly;
};

/* A node in a file system tree */
struct tree_node_t {
	/* Parent directory children linked list pointer. */
	tree_node_t *next;

	/* Root node has this set to NULL. */
	tree_node_t *parent;

	/* For the root node, this points to an empty string. */
	char *name;

	/*
	  A pointer to an extended attribute array or NULL if unused.

	  This field is not stored in-line and taken care of by the generic
	  fstree cleanup code, since it is generatde after the tree already
	  exists and shared across multiple nodes.
	*/
	tree_xattr_t *xattr;

	uint32_t uid;
	uint32_t gid;
	uint16_t mode;

	/* SquashFS inode refernce number. 32 bit offset of the meta data
	   block start (relative to inode table start), shifted left by 16
	   and ored with a 13 bit offset into the uncompressed meta data block.

	   Generated on the fly when writing inodes. */
	uint64_t inode_ref;

	uint32_t inode_num;

	/* Type specific data. Pointers are into payload area blow. */
	union {
		dir_info_t *dir;
		file_info_t *file;
		char *slink_target;
		uint64_t devno;
	} data;

	uint8_t payload[];
};

/* Encapsulates a file system tree */
struct fstree_t {
	uint32_t default_uid;
	uint32_t default_gid;
	uint32_t default_mode;
	uint32_t default_mtime;
	size_t block_size;
	size_t inode_tbl_size;

	str_table_t xattr_keys;
	str_table_t xattr_values;

	tree_node_t *root;
	tree_xattr_t *xattr;

	/* linear array of tree nodes. inode number is array index */
	tree_node_t **inode_table;
};

/*
  Initializing means copying over the default values and creating a root node.
  On error, an error message is written to stderr.

  `block_size` is the the data block size for regular files.

  Returns 0 on success.
*/
int fstree_init(fstree_t *fs, size_t block_size, uint32_t mtime,
		uint16_t default_mode, uint32_t default_uid,
		uint32_t default_gid);

void fstree_cleanup(fstree_t *fs);

/*
  Add a generic node to an fstree.

  The new node is inserted by path. If some components of the path don't
  exist, they are created as directories with default permissions, like
  mkdir -p would, and marked as implcitily created. A subsequent call that
  tries to create an existing tree node will fail, except if the target
  is an implicitly created directory node and the call tries to create it
  as a directory (this will simply overwrite the permissions and ownership).
  The implicitly created flag is then cleared. Subsequent attempts to create
  an existing directory again will then also fail.

  This function does not print anything to stderr, instead it sets an
  appropriate errno value.

  `extra_len` specifies an additional number of bytes to allocate for payload
  data in the tree node.
*/
tree_node_t *fstree_add(fstree_t *fs, const char *path, uint16_t mode,
			uint32_t uid, uint32_t gid, size_t extra_len);

/*
  A wrappter around fstree_add for regular files.

  This function internally computes the number of extra payload bytes
  requiered and sets up the payload pointers propperly.
*/
tree_node_t *fstree_add_file(fstree_t *fs, const char *path, uint16_t mode,
			     uint32_t uid, uint32_t gid, uint64_t filesz,
			     const char *input);

/*
  Add an extended attribute key value pair to a tree node.

  Returns 0 on success, prints error to stderr on failure.
*/
int fstree_add_xattr(fstree_t *fs, tree_node_t *node,
		     const char *key, const char *value);

/* Recompute running index number of all xattr blocks. */
void fstree_xattr_reindex(fstree_t *fs);

/* Sort and dedupliciate xattr blocks, then recumpute the index numbers. */
void fstree_xattr_deduplicate(fstree_t *fs);

/*
  Parses the file format accepted by gensquashfs and produce a file system
  tree from it. File input paths are interpreted as relative to the given
  root dir. If rootdir is NULL, use the path where the input file is as root
  dir.

  This function tries to temporarily change the working directory, so if it
  fails, the current working directory is undefined.

  On failure, an error report with filename and line number is written
  to stderr.

  Returns 0 on success.
 */
int fstree_from_file(fstree_t *fs, const char *filename, const char *rootdir);

/*
  Recursively scan a directory and generate a file system tree from it.

  Returns 0 on success, prints errors to stderr.
 */
int fstree_from_dir(fstree_t *fs, const char *path);

/* Lexicographically sort all directory contents. */
void fstree_sort(fstree_t *fs);

/* Add labels from an SELinux labeling file to all tree nodes.
   Returns 0 on success. Internally prints errors to stderr. */
int fstree_relabel_selinux(fstree_t *fs, const char *filename);

/* Returns 0 on success. Prints to stderr on failure */
int fstree_gen_inode_table(fstree_t *fs);

/*
  Generate a string holding the full path of a node. Returned
  string must be freed.

  Returns NULL on failure and sets errno.
*/
char *fstree_get_path(tree_node_t *node);

/* get a struct stat from a tree node */
void fstree_node_stat(fstree_t *fs, tree_node_t *node, struct stat *sb);

#endif /* FSTREE_H */
