/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_reader.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef DATA_READER_H
#define DATA_READER_H

#include "config.h"

#include "sqfs/compress.h"
#include "sqfs/data.h"
#include "fstree.h"

typedef struct data_reader_t data_reader_t;

/*
  Create a data reader for accessing data blocks in a squashfs image.

  Internally creates a fragment_reader_t (if applicable) to resolve
  fragment indices.

  Prints error messsages to stderr on failure.
 */
data_reader_t *data_reader_create(sqfs_file_t *file, sqfs_super_t *super,
				  sqfs_compressor_t *cmp);

void data_reader_destroy(data_reader_t *data);

/*
  Use a file_info_t to locate and extract all blocks of the coresponding
  file and its fragment, if it has one. The entire data is dumped to the
  given file descriptor.

  If allow_sparse is true, try to truncate and seek forward on outfd if a
  zero block is found. If false, always write blocks of zeros to outfd.

  Returns 0 on success, prints error messages to stderr on failure.
 */
int data_reader_dump_file(data_reader_t *data, file_info_t *fi, int outfd,
			  bool allow_sparse);

/*
  Read a chunk of data from a file. Starting from 'offset' into the
  uncompressed file, read 'size' bytes into 'buffer'.

  Returns the number of bytes read, 0 if EOF, -1 on failure. Prints an
  error message to stderr on failure.
 */
ssize_t data_reader_read(data_reader_t *data, file_info_t *fi,
			 uint64_t offset, void *buffer, size_t size);

#endif /* DATA_READER_H */
