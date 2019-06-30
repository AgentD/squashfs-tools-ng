/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef DATA_WRITER_H
#define DATA_WRITER_H

#include "squashfs.h"
#include "compress.h"
#include "fstree.h"
#include "util.h"

typedef struct data_writer_t data_writer_t;

/*
  Create a data writer. The pointer to the super block is kept internally and
  used to automatically update various counters when writing data.

  Returns NULL on failure and prints errors to stderr.
 */
data_writer_t *data_writer_create(sqfs_super_t *super, compressor_t *cmp,
				  int outfd);

void data_writer_destroy(data_writer_t *data);

/*
  Write the finalfragment table to the underlying file.

  Returns 0 on success, prints errors to stderr.
*/
int data_writer_write_fragment_table(data_writer_t *data);

/*
  Compress and flush the current fragment buffer even if it is not full yet.

  Returns 0 on success, prints errors to stderr.
*/
int data_writer_flush_fragments(data_writer_t *data);

/*
  Read data from the given file descriptor, partition it into blocks and
  write them out (possibly compressed) to the underlying file. If the size
  is not a multiple of the block size, the last bit is kept in an internal
  fragment buffer which is written out if full.

  The file_info_t object is updated accordingly and used to determine the
  number of bytes to write and the input file name to report errors.

  Blocks or fragments that are all zero bytes automatically detected,
  not written out and the sparse file accounting updated accordingly.

  Returns 0 on success, prints errors to stderr.
*/
int write_data_from_fd(data_writer_t *data, file_info_t *fi, int infd);

/*
  Does the same as write_data_from_fd but the input file is the condensed
  representation of a sparse file. The layout must be in order and
  non-overlapping.

  Returns 0 on success, prints errors to stderr.
 */
int write_data_from_fd_condensed(data_writer_t *data, file_info_t *fi,
				 int infd, sparse_map_t *map);

#endif /* DATA_WRITER_H */
