/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef FRAG_READER_H
#define FRAG_READER_H

#include "squashfs.h"
#include "compress.h"

#include <stdint.h>
#include <stddef.h>

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

frag_reader_t *frag_reader_create(sqfs_super_t *super, int fd,
				  compressor_t *cmp);

void frag_reader_destroy(frag_reader_t *f);

int frag_reader_read(frag_reader_t *f, size_t index, size_t offset,
		     void *buffer, size_t size);

#endif /* FRAG_READER_H */
