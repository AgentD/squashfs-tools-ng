/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef META_WRITER_H
#define META_WRITER_H

#include "compress.h"
#include "squashfs.h"

typedef struct {
	size_t offset;
	size_t block_offset;
	int outfd;
	compressor_t *cmp;
	uint8_t data[SQFS_META_BLOCK_SIZE + 2];
	uint8_t scratch[SQFS_META_BLOCK_SIZE + 2];
} meta_writer_t;

meta_writer_t *meta_writer_create(int fd, compressor_t *cmp);

void meta_writer_destroy(meta_writer_t *m);

int meta_writer_flush(meta_writer_t *m);

int meta_writer_append(meta_writer_t *m, const void *data, size_t size);

#endif /* META_WRITER_H */
