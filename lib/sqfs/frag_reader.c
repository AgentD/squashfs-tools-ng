/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "meta_reader.h"
#include "frag_reader.h"
#include "util.h"

#include <unistd.h>
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
	size_t i, blockcount, j, diff, count;
	sqfs_fragment_t *tbl = NULL;
	uint64_t *locations = NULL;
	meta_reader_t *m = NULL;
	frag_reader_t *f = NULL;

	count = super->fragment_entry_count;
	blockcount = count / (SQFS_META_BLOCK_SIZE / sizeof(tbl[0]));

	if (count % (SQFS_META_BLOCK_SIZE / sizeof(tbl[0])))
		++blockcount;

	/* pre allocate all the stuff */
	f = calloc(1, sizeof(*f) + super->block_size * 2);
	if (f == NULL)
		goto fail_rd;

	tbl = calloc(count, sizeof(tbl[0]));
	if (tbl == NULL)
		goto fail_rd;

	locations = malloc(blockcount * sizeof(locations[0]));
	if (locations == NULL)
		goto fail_rd;

	/* read the meta block offset table */
	if (read_data_at("reading fragment table", super->fragment_table_start,
			 fd, locations, blockcount * sizeof(locations[0]))) {
		goto fail;
	}

	for (i = 0; i < blockcount; ++i)
		locations[i] = le64toh(locations[i]);

	/* read the meta blocks */
	m = meta_reader_create(fd, cmp);
	if (m == NULL)
		goto fail;

	for (i = 0, j = 0; i < blockcount && j < count; ++i, j += diff) {
		if (meta_reader_seek(m, locations[i], 0))
			goto fail;

		diff = SQFS_META_BLOCK_SIZE / sizeof(tbl[0]);
		if (diff > count)
			diff = count;

		if (meta_reader_read(m, tbl + j, diff * sizeof(tbl[0])))
			goto fail;
	}

	for (i = 0; i < count; ++i) {
		tbl[i].start_offset = le64toh(tbl[i].start_offset);
		tbl[i].size = le32toh(tbl[i].size);
	}

	/* cleanup and ship it */
	meta_reader_destroy(m);
	free(locations);

	f->tbl = tbl;
	f->num_fragments = count;
	f->cmp = cmp;
	f->fd = fd;
	f->block_size = super->block_size;
	f->current_index = count;
	return f;
fail_rd:
	perror("reading fragment table");
	goto fail;
fail:
	if (m != NULL)
		meta_reader_destroy(m);
	free(tbl);
	free(locations);
	free(f);
	return NULL;
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
