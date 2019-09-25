/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSTREE_H
#define FSTREE_H

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "str_table.h"

#define FSTREE_XATTR_KEY_BUCKETS 31
#define FSTREE_XATTR_VALUE_BUCKETS 511

typedef struct tree_node_t tree_node_t;
typedef struct file_info_t file_info_t;
typedef struct dir_info_t dir_info_t;
typedef struct fstree_t fstree_t;
typedef struct tree_xattr_t tree_xattr_t;

enum {
	DIR_SCAN_KEEP_TIME = 0x01,

	DIR_SCAN_ONE_FILESYSTEM = 0x02,

	DIR_SCAN_READ_XATTR = 0x04,
};

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

	/* Array with pairs of key-value tupples */
	struct {
		uint32_t key_index;
		uint32_t value_index;
	} attr[];
};

/* Additional meta data stored in a tree_node_t for regular files. */
struct file_info_t {
	/* Linked list pointer for files in fstree_t */
	file_info_t *next;

	/* Path to the input file. */
	char *input_file;

	void *user_ptr;
};

/* Additional meta data stored in a tree_node_t for directories */
struct dir_info_t {
	/* Linked list head for children in the directory */
	tree_node_t *children;

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
	uint32_t inode_num;
	uint32_t mod_time;
	uint16_t mode;
	uint16_t pad0[3];

	/* SquashFS inode refernce number. 32 bit offset of the meta data
	   block start (relative to inode table start), shifted left by 16
	   and ored with a 13 bit offset into the uncompressed meta data block.

	   Generated on the fly when writing inodes. */
	uint64_t inode_ref;

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
	struct stat defaults;
	size_t block_size;
	size_t inode_tbl_size;

	str_table_t xattr_keys;
	str_table_t xattr_values;

	tree_node_t *root;
	tree_xattr_t *xattr;

	/* linear array of tree nodes. inode number is array index */
	tree_node_t **inode_table;

	/* linear linked list of all regular files */
	file_info_t *files;
};

/*
  Initializing means copying over the default values and creating a root node.
  On error, an error message is written to stderr.

  `block_size` is the the data block size for regular files.

  The string `defaults` can specify default attributes (mode, uid, gid, mtime)
  as a comma seperated list of key value paris (<key>=<value>[,...]). The string
  is passed to getsubopt and will be altered.

  Returns 0 on success.
*/
int fstree_init(fstree_t *fs, size_t block_size, char *defaults);

void fstree_cleanup(fstree_t *fs);

/*
  Create a tree node from a struct stat, node name and extra data.

  For symlinks, the extra part is interpreted as target. For regular files, it
  is interpreted as input path (can be NULL). The name doesn't have to be null
  terminated, a length has to be specified.

  This function does not print anything to stderr, instead it sets an
  appropriate errno value.

  The resulting node can be freed with a single free() call.
*/
tree_node_t *fstree_mknode(tree_node_t *parent, const char *name,
			   size_t name_len, const char *extra,
			   const struct stat *sb);

/*
  Add a node to an fstree at a specific path.

  If some components of the path don't exist, they are created as directories
  with default permissions, like mkdir -p would, and marked as implcitily
  created. A subsequent call that tries to create an existing tree node will
  fail, except if the target is an implicitly created directory node and the
  call tries to create it as a directory (this will simply overwrite the
  permissions and ownership). The implicitly created flag is then cleared.
  Subsequent attempts to create an existing directory again will then also
  fail.

  This function does not print anything to stderr, instead it sets an
  appropriate errno value. Internally it uses fstree_mknode to create the node.
*/
tree_node_t *fstree_add_generic(fstree_t *fs, const char *path,
				const struct stat *sb, const char *extra);

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
  tree from it. File input paths are interpreted as relative to the current
  working directory.

  Data is read from the given file pointer. The filename is only used for
  producing error messages.

  On failure, an error report with filename and line number is written
  to stderr.

  Returns 0 on success.
 */
int fstree_from_file(fstree_t *fs, const char *filename, FILE *fp);

/*
  Recursively scan a directory and generate a file system tree from it.

  Returns 0 on success, prints errors to stderr.
 */
int fstree_from_dir(fstree_t *fs, const char *path, unsigned int flags);

/* Add labels from an SELinux labeling file to all tree nodes.
   Returns 0 on success. Internally prints errors to stderr. */
int fstree_relabel_selinux(fstree_t *fs, const char *filename);

/* Returns 0 on success. Prints to stderr on failure */
int fstree_gen_inode_table(fstree_t *fs);

void fstree_gen_file_list(fstree_t *fs);

/*
  Generate a string holding the full path of a node. Returned
  string must be freed.

  Returns NULL on failure and sets errno.
*/
char *fstree_get_path(tree_node_t *node);

/* ASCIIbetically sort a linked list of tree nodes */
tree_node_t *tree_node_list_sort(tree_node_t *head);

/* ASCIIbetically sort all sub directories recursively */
void tree_node_sort_recursive(tree_node_t *root);

/*
  If the environment variable SOURCE_DATE_EPOCH is set to a parsable number
  that fits into an unsigned 32 bit value, return its value. Otherwise,
  default to 0.
 */
uint32_t get_source_date_epoch(void);

#endif /* FSTREE_H */
