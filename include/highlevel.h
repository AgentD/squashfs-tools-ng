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
#include "sqfs/dir.h"
#include "sqfs/io.h"
#include "data_reader.h"
#include "data_writer.h"
#include "fstree.h"

#include <stdint.h>
#include <stddef.h>

typedef struct {
	sqfs_compressor_t *cmp;
	data_reader_t *data;
	sqfs_file_t *file;
	sqfs_super_t super;
	fstree_t fs;
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
int sqfs_serialize_fstree(sqfs_file_t *file, sqfs_super_t *super, fstree_t *fs,
			  sqfs_compressor_t *cmp, sqfs_id_table_t *idtbl);

/*
  Convert a generic squashfs tree node to an fstree_t node.

  Prints error messages to stderr on failure.
 */
tree_node_t *tree_node_from_inode(sqfs_inode_generic_t *inode,
				  const sqfs_id_table_t *idtbl,
				  const char *name);

/*
  Restore a file system tree from a squashfs image. The given flags are a
  combination of RDTREE_FLAGS.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int deserialize_fstree(fstree_t *out, sqfs_super_t *super,
		       sqfs_compressor_t *cmp, sqfs_file_t *file, int flags);

/*
  Generate a squahfs xattr table from a file system tree.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int write_xattr(sqfs_file_t *file, fstree_t *fs, sqfs_super_t *super,
		sqfs_compressor_t *cmp);

/*
  Generate an NFS export table.

  Returns 0 on success. Prints error messages to stderr on failure.
 */
int write_export_table(sqfs_file_t *file, fstree_t *fs, sqfs_super_t *super,
		       sqfs_compressor_t *cmp);

/* Print out fancy statistics for squashfs packing tools */
void sqfs_print_statistics(sqfs_super_t *super, data_writer_stats_t *stats);

/* Open a squashfs file, extract all the information we may need and
   construct datastructures we need to access its contents.
   Returns 0 on success. Prints error messages to stderr on failure.
*/
int sqfs_reader_open(sqfs_reader_t *rd, const char *filename,
		     int rdtree_flags);

/* Cleanup after a successfull sqfs_reader_open */
void sqfs_reader_close(sqfs_reader_t *rd);

void compressor_print_available(void);

E_SQFS_COMPRESSOR compressor_get_default(void);

int compressor_cfg_init_options(sqfs_compressor_config_t *cfg,
				E_SQFS_COMPRESSOR id,
				size_t block_size, char *options);

void compressor_print_help(E_SQFS_COMPRESSOR id);

int xattr_reader_restore_node(sqfs_xattr_reader_t *xr, fstree_t *fs,
			      tree_node_t *node, uint32_t xattr);

sqfs_inode_generic_t *tree_node_to_inode(fstree_t *fs, sqfs_id_table_t *idtbl,
					 tree_node_t *node);

#endif /* HIGHLEVEL_H */
