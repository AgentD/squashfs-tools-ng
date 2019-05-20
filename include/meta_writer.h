/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef META_WRITER_H
#define META_WRITER_H

#include "compress.h"
#include "squashfs.h"

typedef struct meta_writer_t meta_writer_t;

/* Create a meta data reader using a given compressor to compress data.
   Internally prints error message to stderr on failure. */
meta_writer_t *meta_writer_create(int fd, compressor_t *cmp);

void meta_writer_destroy(meta_writer_t *m);

/* Compress and flush the currently unfinished block to disk. Returns 0 on
   success, internally prints error message to stderr on failure */
int meta_writer_flush(meta_writer_t *m);

/* Returns 0 on success. Prints error message to stderr on failure. */
int meta_writer_append(meta_writer_t *m, const void *data, size_t size);

/* Query the current block start position and offset within the block */
void meta_writer_get_position(const meta_writer_t *m, uint64_t *block_start,
			      uint32_t *offset);

/* Reset all internal state, including the current block start position. */
void meta_writer_reset(meta_writer_t *m);

#endif /* META_WRITER_H */
