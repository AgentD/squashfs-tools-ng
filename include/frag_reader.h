/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FRAG_READER_H
#define FRAG_READER_H

#include "squashfs.h"
#include "compress.h"

#include <stdint.h>
#include <stddef.h>

/* A simple interface for accessing fragments in a SquashFS image */
typedef struct {
	sqfs_fragment_t *tbl;
	size_t num_fragments;

	int fd;
	compressor_t *cmp;
	size_t block_size;
	size_t used;
	size_t current_index;
	uint8_t buffer[];
} frag_reader_t;

/* Reads and decodes the fragment table from the image. Cleans up after itself
   and prints error messages to stderr on the way if it fails. */
frag_reader_t *frag_reader_create(sqfs_super_t *super, int fd,
				  compressor_t *cmp);

void frag_reader_destroy(frag_reader_t *f);

/*
  Read data from a fragment.

  The function takes care of loading and uncompressing the fragment block
  (which is skipped if it has already been loaded earlier).

  `index` is a fragment index from an inode, `offset` is a byte offset into
  the fragment block and `size` is the number of bytes to copy into the
  destination buffer.

  Returns 0 on success. Prints error messages to stderr on failure (including
  index out of bounds).
*/
int frag_reader_read(frag_reader_t *f, size_t index, size_t offset,
		     void *buffer, size_t size);

#endif /* FRAG_READER_H */
