/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "frag_reader.h"
#include "highlevel.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct frag_reader_t {
	sqfs_fragment_t *tbl;
	size_t num_fragments;

	int fd;
	compressor_t *cmp;
	size_t block_size;
	size_t used;
	size_t current_index;
	uint8_t buffer[];
};

static int precache_block(frag_reader_t *f, size_t i)
{
	bool compressed;
	size_t size;
	ssize_t ret;

	if (i == f->current_index)
		return 0;

	compressed = (f->tbl[i].size & (1 << 24)) == 0;
	size = f->tbl[i].size & ((1 << 24) - 1);

	if (size > f->block_size) {
		fputs("found fragment block larger than block size\n", stderr);
		return -1;
	}

	if (read_data_at("reading fragment", f->tbl[i].start_offset,
			 f->fd, f->buffer, size)) {
		return -1;
	}

	if (compressed) {
		ret = f->cmp->do_block(f->cmp, f->buffer, size,
				       f->buffer + f->block_size, f->block_size);

		if (ret <= 0) {
			fputs("extracting fragment failed\n", stderr);
			return -1;
		}

		size = ret;
		memmove(f->buffer, f->buffer + f->block_size, ret);
	}

	f->current_index = i;
	f->used = size;
	return 0;
}


frag_reader_t *frag_reader_create(sqfs_super_t *super, int fd,
				  compressor_t *cmp)
{
	frag_reader_t *f = NULL;
	size_t i;

	f = calloc(1, sizeof(*f) + super->block_size * 2);
	if (f == NULL) {
		perror("creating fragment table");
		return NULL;
	}

	f->block_size = super->block_size;
	f->num_fragments = super->fragment_entry_count;
	f->current_index = f->num_fragments;
	f->cmp = cmp;
	f->fd = fd;

	f->tbl = sqfs_read_table(fd, cmp, sizeof(f->tbl[0]) * f->num_fragments,
				 super->fragment_table_start);
	if (f->tbl == NULL) {
		free(f);
		return NULL;
	}

	for (i = 0; i < f->num_fragments; ++i) {
		f->tbl[i].start_offset = le64toh(f->tbl[i].start_offset);
		f->tbl[i].size = le32toh(f->tbl[i].size);
	}

	return f;
}

void frag_reader_destroy(frag_reader_t *f)
{
	free(f->tbl);
	free(f);
}

int frag_reader_read(frag_reader_t *f, size_t index, size_t offset,
		     void *buffer, size_t size)
{
	if (precache_block(f, index))
		return -1;

	if (offset >= f->used)
		goto fail_range;

	if (size == 0)
		return 0;

	if ((offset + size - 1) >= f->used)
		goto fail_range;

	memcpy(buffer, f->buffer + offset, size);
	return 0;
fail_range:
	fputs("attempted to read past fragment block limits\n", stderr);
	return -1;
}

const sqfs_fragment_t *frag_reader_get_table(const frag_reader_t *f)
{
	return f->tbl;
}

size_t frag_reader_get_fragment_count(const frag_reader_t *f)
{
	return f->num_fragments;
}
