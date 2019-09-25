/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef DATA_WRITER_H
#define DATA_WRITER_H

#include "config.h"

#include "sqfs/compress.h"
#include "sqfs/data.h"
#include "fstree.h"
#include "util.h"

typedef struct data_writer_t data_writer_t;

typedef struct {
	size_t file_count;
	size_t blocks_written;
	size_t frag_blocks_written;
	size_t duplicate_blocks;
	size_t sparse_blocks;
	size_t frag_count;
	size_t frag_dup;
	uint64_t bytes_written;
	uint64_t bytes_read;
} data_writer_stats_t;

enum {
	/* Don't generate fragments, always write the last block to disk as a
	   block, even if it is incomplete. */
	DW_DONT_FRAGMENT = 0x01,

	/* Intentionally write all blocks uncompressed. */
	DW_DONT_COMPRESS = 0x02,

	/* Make sure the first block of a file is alligned to
	   device block size */
	DW_ALLIGN_DEVBLK = 0x04,
};

/*
  Create a data writer. The pointer to the super block is kept internally and
  used to automatically update various counters when writing data.

  Returns NULL on failure and prints errors to stderr.
 */
data_writer_t *data_writer_create(sqfs_super_t *super, sqfs_compressor_t *cmp,
				  sqfs_file_t *file, size_t devblksize,
				  unsigned int num_jobs, size_t max_backlog);

void data_writer_destroy(data_writer_t *data);

/*
  Write the finalfragment table to the underlying file.

  Returns 0 on success, prints errors to stderr.
*/
int data_writer_write_fragment_table(data_writer_t *data);

/*
  Wait for everything to be written to disk. This also forces a currently
  pending fragment block to be compressed and wrtten.

  Returns 0 on success, prints errors to stderr.
*/
int data_writer_sync(data_writer_t *data);

/*
  Read data from the given file descriptor, partition it into blocks and
  write them out (possibly compressed) to the underlying file. If the size
  is not a multiple of the block size, the last bit is kept in an internal
  fragment buffer which is written out if full.

  The file_info_t object is updated accordingly and used to determine the
  number of bytes to write and the input file name to report errors.

  Blocks or fragments that are all zero bytes automatically detected,
  not written out and the sparse file accounting updated accordingly.

  The flags argument is a combination of DW_* flags. After completion the
  data writer collects the 'fi' in an internal list it uses for deduplication.

  Returns 0 on success, prints errors to stderr.
*/
int write_data_from_file(data_writer_t *data, sqfs_inode_generic_t *inode,
			 sqfs_file_t *file, int flags);

/*
  Does the same as write_data_from_fd but the input file is the condensed
  representation of a sparse file. The layout must be in order and
  non-overlapping.

  The flags argument is a combination of DW_* flags. After completion the
  data writer collects the 'fi' in an internal list it uses for deduplication.

  Returns 0 on success, prints errors to stderr.
 */
int write_data_from_file_condensed(data_writer_t *data, sqfs_file_t *file,
				   sqfs_inode_generic_t *inode,
				   const sqfs_sparse_map_t *map, int flags);

data_writer_stats_t *data_writer_get_stats(data_writer_t *data);

#endif /* DATA_WRITER_H */
