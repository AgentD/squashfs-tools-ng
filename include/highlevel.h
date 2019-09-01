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
#include "data_reader.h"
#include "fstree.h"

#include <stdint.h>
#include <stddef.h>

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
  Convenience function for writing meta data to a SquashFS image

  This function internally creates a meta data writer and writes the given
  'data' blob with 'table_size' bytes to disk, neatly partitioned into meta
  data blocks. For each meta data block, it remembers the 64 bit start address,
  writes out all addresses to an uncompressed list and returns the location
  where the address list starts in 'start'.

  Returns 0 on success. Internally prints error messages to stderr.
 */
int sqfs_write_table(int outfd, sqfs_super_t *super, compressor_t *cmp,
		     const void *data, size_t table_size, uint64_t *start);

void *sqfs_read_table(int fd, compressor_t *cmp, size_t table_size,
		      uint64_t location, uint64_t lower_limit,
		      uint64_t upper_limit);

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

#endif /* HIGHLEVEL_H */
