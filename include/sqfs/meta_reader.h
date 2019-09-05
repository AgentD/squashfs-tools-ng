/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * meta_reader.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_META_READER_H
#define SQFS_META_READER_H

#include "config.h"

#include "sqfs/compress.h"
#include "sqfs/data.h"

typedef struct sqfs_meta_reader_t sqfs_meta_reader_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Create a meta data reader using a given compressor to extract data.
   Internally prints error message to stderr on failure.

   Start offset and limit can be specified to do bounds checking against
   a subregion of the filesystem image.
*/
sqfs_meta_reader_t *sqfs_meta_reader_create(int fd, sqfs_compressor_t *cmp,
					    uint64_t start, uint64_t limit);

void sqfs_meta_reader_destroy(sqfs_meta_reader_t *m);

/* Returns 0 on success. Internally prints to stderr on failure */
int sqfs_meta_reader_seek(sqfs_meta_reader_t *m, uint64_t block_start,
			  size_t offset);

void sqfs_meta_reader_get_position(sqfs_meta_reader_t *m,
				   uint64_t *block_start,
				   size_t *offset);

/* Returns 0 on success. Internally prints to stderr on failure */
int sqfs_meta_reader_read(sqfs_meta_reader_t *m, void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* SQFS_META_READER_H */
