/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * meta_reader.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef META_READER_H
#define META_READER_H

#include "config.h"

#include "sqfs/compress.h"
#include "sqfs/inode.h"
#include "sqfs/data.h"
#include "sqfs/dir.h"

typedef struct meta_reader_t meta_reader_t;

/* Create a meta data reader using a given compressor to extract data.
   Internally prints error message to stderr on failure.

   Start offset and limit can be specified to do bounds checking against
   a subregion of the filesystem image.
*/
meta_reader_t *meta_reader_create(int fd, compressor_t *cmp,
				  uint64_t start, uint64_t limit);

void meta_reader_destroy(meta_reader_t *m);

/* Returns 0 on success. Internally prints to stderr on failure */
int meta_reader_seek(meta_reader_t *m, uint64_t block_start,
		     size_t offset);

void meta_reader_get_position(meta_reader_t *m, uint64_t *block_start,
			      size_t *offset);

/* Returns 0 on success. Internally prints to stderr on failure */
int meta_reader_read(meta_reader_t *m, void *data, size_t size);

/* Inode can be freed with a single free() call.
   The function internally prints error message to stderr on failure. */
sqfs_inode_generic_t *meta_reader_read_inode(meta_reader_t *ir,
					     sqfs_super_t *super,
					     uint64_t block_start,
					     size_t offset);

/* Returns 0 on success. Internally prints to stderr on failure */
int meta_reader_read_dir_header(meta_reader_t *m, sqfs_dir_header_t *hdr);

/* Entry can be freed with a single free() call.
   The function internally prints to stderr on failure */
sqfs_dir_entry_t *meta_reader_read_dir_ent(meta_reader_t *m);

#endif /* META_READER_H */
