/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef META_READER_H
#define META_READER_H

#include "compress.h"
#include "squashfs.h"

typedef struct {
	uint64_t block_offset;
	uint64_t next_block;
	size_t offset;
	int fd;
	compressor_t *cmp;
	uint8_t data[SQFS_META_BLOCK_SIZE];
	uint8_t scratch[SQFS_META_BLOCK_SIZE];
} meta_reader_t;

meta_reader_t *meta_reader_create(int fd, compressor_t *cmp);

void meta_reader_destroy(meta_reader_t *m);

int meta_reader_seek(meta_reader_t *m, uint64_t block_start,
		     size_t offset);

int meta_reader_read(meta_reader_t *m, void *data, size_t size);

sqfs_inode_generic_t *meta_reader_read_inode(meta_reader_t *ir,
					     sqfs_super_t *super,
					     uint64_t block_start,
					     size_t offset);

int meta_reader_read_dir_header(meta_reader_t *m, sqfs_dir_header_t *hdr);

sqfs_dir_entry_t *meta_reader_read_dir_ent(meta_reader_t *m);

#endif /* META_READER_H */
