/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef HIGHLEVEL_H
#define HIGHLEVEL_H

#include "squashfs.h"
#include "compress.h"
#include "id_table.h"
#include "fstree.h"

#include <stdint.h>
#include <stddef.h>

enum RDTREE_FLAGS {
	RDTREE_NO_DEVICES = 0x01,
	RDTREE_NO_SOCKETS = 0x02,
	RDTREE_NO_FIFO = 0x04,
	RDTREE_NO_SLINKS = 0x08,
	RDTREE_NO_EMPTY = 0x10,
};

/*
  Convenience function for writing meta data to a SquashFS image

  This function internally creates a meta data writer and writes 'count'
  blocks of data from 'data' to it, each 'entsize' bytes in size. For each
  meta data block, it remembers the 64 bit start address, writes out all
  addresses to an uncompressed address list and returns the location where
  the address list starts.

  Returns 0 on success. Internally prints error messages to stderr.
 */
int sqfs_write_table(int outfd, sqfs_super_t *super, const void *data,
		     size_t entsize, size_t count, uint64_t *startblock,
		     compressor_t *cmp);

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

#endif /* HIGHLEVEL_H */
