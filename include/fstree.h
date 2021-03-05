/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSTREE_H
#define FSTREE_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "sqfs/predef.h"
#include "compat.h"

enum {
	DIR_SCAN_KEEP_TIME = 0x01,

	DIR_SCAN_ONE_FILESYSTEM = 0x02,

	DIR_SCAN_NO_RECURSION = 0x04,

	DIR_SCAN_NO_SOCK = 0x0008,
	DIR_SCAN_NO_SLINK = 0x0010,
	DIR_SCAN_NO_FILE = 0x0020,
	DIR_SCAN_NO_BLK = 0x0040,
	DIR_SCAN_NO_DIR = 0x0080,
	DIR_SCAN_NO_CHR = 0x0100,
	DIR_SCAN_NO_FIFO = 0x0200,
};

#define FSTREE_MODE_HARD_LINK (0)
#define FSTREE_MODE_HARD_LINK_RESOLVED (1)

typedef struct tree_node_t tree_node_t;
typedef struct file_info_t file_info_t;
typedef struct dir_info_t dir_info_t;
typedef struct fstree_t fstree_t;

/*
  Optionally used by fstree_from_dir and fstree_from_subdir to
  execute custom actions for each discovered node.

  If it returns a value > 0, the new node is discarded, if < 0, scanning is
  aborted and returns a failure status.
 */
typedef int (*scan_node_callback)(void *user, fstree_t *fs, tree_node_t *node);

/* Additional meta data stored in a tree_node_t for regular files. */
struct file_info_t {
	/* Linked list pointer for files in fstree_t */
	file_info_t *next;

	/* Path to the input file. */
	char *input_file;

	sqfs_inode_generic_t *inode;
};

/* Additional meta data stored in a tree_node_t for directories */
struct dir_info_t {
	/* Linked list head for children in the directory */
	tree_node_t *children;

	/* Set to true for implicitly generated directories.  */
	bool created_implicitly;

	/* Used by recursive tree walking code to avoid hard link loops */
	bool visited;
};

/* A node in a file system tree */
struct tree_node_t {
	/* Parent directory children linked list pointer. */
	tree_node_t *next;

	/* Root node has this set to NULL. */
	tree_node_t *parent;

	/* For the root node, this points to an empty string. */
	char *name;

	sqfs_u32 xattr_idx;
	sqfs_u32 uid;
	sqfs_u32 gid;
	sqfs_u32 inode_num;
	sqfs_u32 mod_time;
	sqfs_u16 mode;
	sqfs_u16 link_count;

	/* SquashFS inode refernce number. 32 bit offset of the meta data
	   block start (relative to inode table start), shifted left by 16
	   and ored with a 13 bit offset into the uncompressed meta data block.

	   Generated on the fly when writing inodes. */
	sqfs_u64 inode_ref;

	/* Type specific data. "target" pointer is into payload area below. */
	union {
		dir_info_t dir;
		file_info_t file;
		char *target;
		sqfs_u64 devno;
		tree_node_t *target_node;
	} data;

	sqfs_u8 payload[];
};

/* Encapsulates a file system tree */
struct fstree_t {
	struct stat defaults;
	size_t unique_inode_count;

	/* flat array of all nodes that have an inode number */
	tree_node_t **inodes;

	tree_node_t *root;

	/* linear linked list of all regular files */
	file_info_t *files;
};

/*
  Initializing means copying over the default values and creating a root node.
  On error, an error message is written to stderr.

  The string `defaults` can specify default attributes (mode, uid, gid, mtime)
  as a comma separated list of key value paris (<key>=<value>[,...]). The string
  is passed to getsubopt and will be altered.

  Returns 0 on success.
*/
int fstree_init(fstree_t *fs, char *defaults);

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
  Parses the file format accepted by gensquashfs and produce a file system
  tree from it. File input paths are interpreted as relative to the current
  working directory.

  On failure, an error report with filename and line number is written
  to stderr.

  Returns 0 on success.
 */
int fstree_from_file(fstree_t *fs, const char *filename,
		     const char *basepath);

/*
  This function performs all the necessary post processing steps on the file
  system tree, i.e. recursively sorting all directory entries by name,
  allocating inode numbers, resolving hard links and stringing all files nodes
  together into a linked list.

  The total inode count is stored in unique_inode_count. The head of the file
  list is pointed to by fs->files.

  The "inodes" array is allocated and each node that has an inode number is
  mapped into the array at index inode_num - 1.

  Returns 0 on success, prints to stderr on failure.
 */
int fstree_post_process(fstree_t *fs);

/*
  Generate a string holding the full path of a node. Returned
  string must be freed.

  Returns NULL on failure and sets errno.
*/
char *fstree_get_path(tree_node_t *node);

/*
  Resolve a path to a tree node. Returns NULL on failure and sets errno.

  If "create_implicitly" is set to true, the function acts essentially like
  mkdir_p if part of the path doesn't exist yet.

  If "stop_at_parent" is true, the function stops at the last component and
  returns the parent or would-be-parent of the last path component, but doesn't
  check if the last path component exists or not.
 */
tree_node_t *fstree_get_node_by_path(fstree_t *fs, tree_node_t *root,
				     const char *path, bool create_implicitly,
				     bool stop_at_parent);

/*
  Convert back to forward slashed, remove all preceeding and trailing slashes,
  collapse all sequences of slashes, remove all path components that are '.'
  and returns failure state if one of the path components is '..'.

  Returns 0 on success.
*/
int canonicalize_name(char *filename);

/*
  Returns true if a given filename is sane, false if it is not (e.g. contains
  slashes or it is equal to '.' or '..').

  If check_os_specific is true, this also checks if the filename contains
  a character, or is equal to a name, that is black listed on the current OS.
  E.g. on Windows, a file named "COM0" or "AUX" is a no-no.
 */
bool is_filename_sane(const char *name, bool check_os_specific);

/*
  Add a hard link node. Returns NULL on failure and sets errno.
 */
tree_node_t *fstree_add_hard_link(fstree_t *fs, const char *path,
				  const char *target);

/*
  Resolve a hard link node and replace it with a direct pointer to the target.

  Returns 0 on success. On failure, errno is set.
 */
int fstree_resolve_hard_link(fstree_t *fs, tree_node_t *node);

/*
  Recursively scan a directory to build a file system tree.

  Returns 0 on success, prints to stderr on failure.
 */
int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, scan_node_callback cb, void *user,
		    unsigned int flags);

/*
  Same as fstree_from_dir, but scans a sub-directory inside the specified path.

  Returns 0 on success, prints to stderr on failure.
 */
int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       scan_node_callback cb, void *user, unsigned int flags);

#endif /* FSTREE_H */
