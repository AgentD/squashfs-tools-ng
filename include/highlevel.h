/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * highlevel.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef HIGHLEVEL_H
#define HIGHLEVEL_H

#include "config.h"

#include "sqfs/compress.h"
#include "sqfs/id_table.h"
#include "sqfs/inode.h"
#include "sqfs/data.h"
#include "sqfs/table.h"
#include "sqfs/meta_writer.h"
#include "sqfs/xattr.h"
#include "data_reader.h"
#include "fstree.h"

#include <stdint.h>
#include <stddef.h>

typedef struct {
	tree_node_t *node;
	uint32_t block;
	uint32_t index;
} idx_ref_t;

typedef struct {
	size_t num_nodes;
	size_t max_nodes;
	idx_ref_t idx_nodes[];
} dir_index_t;

typedef struct {
	compressor_t *cmp;
	data_reader_t *data;
	sqfs_super_t super;
	fstree_t fs;
	int sqfsfd;
} sqfs_reader_t;

enum RDTREE_FLAGS {
	RDTREE_NO_DEVICES = 0x01,
	RDTREE_NO_SOCKETS = 0x02,
	RDTREE_NO_FIFO = 0x04,
	RDTREE_NO_SLINKS = 0x08,
	RDTREE_NO_EMPTY = 0x10,
	RDTREE_READ_XATTR = 0x20,
};

/*
  High level helper function to serialize an entire file system tree to
  a squashfs inode table and directory table.

  The data is written to the given file descriptor and the super block is
  update accordingly (inode and directory table start and total size).

  The function internally creates two meta data writers and uses
  meta_writer_write_inode to serialize the inode table of the fstree.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int sqfs_serialize_fstree(int outfd, sqfs_super_t *super, fstree_t *fs,
			  compressor_t *cmp, id_table_t *idtbl);

/*
  Convert a generic squashfs tree node to an fstree_t node.

  Prints error messages to stderr on failure.
 */
tree_node_t *tree_node_from_inode(sqfs_inode_generic_t *inode,
				  const id_table_t *idtbl,
				  const char *name,
				  size_t block_size);

/*
  Restore a file system tree from a squashfs image. The given flags are a
  combination of RDTREE_FLAGS.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int deserialize_fstree(fstree_t *out, sqfs_super_t *super, compressor_t *cmp,
		       int fd, int flags);

/*
  Generate a squahfs xattr table from a file system tree.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int write_xattr(int outfd, fstree_t *fs, sqfs_super_t *super,
		compressor_t *cmp);

/*
  Generate an NFS export table.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int write_export_table(int outfd, fstree_t *fs, sqfs_super_t *super,
		       compressor_t *cmp);

/* Print out fancy statistics for squashfs packing tools */
void sqfs_print_statistics(fstree_t *fs, sqfs_super_t *super);

/* Open a squashfs file, extract all the information we may need and
   construct datastructures we need to access its contents.
   Returns 0 on success. Prints error messages to stderr on failure.
*/
int sqfs_reader_open(sqfs_reader_t *rd, const char *filename,
		     int rdtree_flags);

/* Cleanup after a successfull sqfs_reader_open */
void sqfs_reader_close(sqfs_reader_t *rd);

/*
  High level helper function that writes squashfs directory entries to
  a meta data writer.

  The dir_info_t structure is used to generate the listing and updated
  accordingly (such as writing back the header position and total size).
  A directory index is created on the fly and returned in *index.
  A single free() call is sufficient.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int meta_writer_write_dir(meta_writer_t *dm, dir_info_t *dir,
			  dir_index_t **index);

/*
  High level helper function to serialize a tree_node_t to a squashfs inode
  and write it to a meta data writer.

  The inode is written to `im`. If it is a directory node, the directory
  contents are written to `dm` using meta_writer_write_dir. The given
  id_table_t is used to store the uid and gid on the fly and write the
  coresponding indices to the inode structure.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int meta_writer_write_inode(fstree_t *fs, id_table_t *idtbl, meta_writer_t *im,
			    meta_writer_t *dm, tree_node_t *node);

void compressor_print_available(void);

E_SQFS_COMPRESSOR compressor_get_default(void);

int compressor_cfg_init_options(compressor_config_t *cfg, E_SQFS_COMPRESSOR id,
				size_t block_size, char *options);

void compressor_print_help(E_SQFS_COMPRESSOR id);

int xattr_reader_restore_node(xattr_reader_t *xr, fstree_t *fs,
			      tree_node_t *node, uint32_t xattr);

#endif /* HIGHLEVEL_H */
