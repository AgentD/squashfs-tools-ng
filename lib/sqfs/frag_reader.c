/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_reader.h"
#include "frag_reader.h"
#include "util.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int precache_block(frag_reader_t *f, size_t i)
{
	bool compressed;
	size_t size;
	ssize_t ret;

	if (i == f->current_index)
		return 0;

	if (lseek(f->fd, f->tbl[i].start_offset, SEEK_SET) == (off_t)-1) {
		perror("seeking to fragment location");
		return -1;
	}

	compressed = (f->tbl[i].size & (1 << 24)) == 0;
	size = f->tbl[i].size & ((1 << 24) - 1);

	if (size > f->block_size) {
		fputs("found fragment block larger than block size\n", stderr);
		return -1;
	}

	ret = read_retry(f->fd, f->buffer, size);
	if (ret < 0) {
		perror("reading fragment");
		return -1;
	}

	if ((size_t)ret < size) {
		fputs("reading fragment: unexpected end of file\n", stderr);
		return -1;
	}

	if (compressed) {
		ret = f->cmp->do_block(f->cmp, f->buffer, size);

		if (ret <= 0) {
			fputs("extracting fragment failed\n", stderr);
			return -1;
		}
	}

	f->current_index = i;
	f->used = ret;
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
	ssize_t ret;

	count = super->fragment_entry_count;
	blockcount = count / (SQFS_META_BLOCK_SIZE / sizeof(tbl[0]));

	if (count % (SQFS_META_BLOCK_SIZE / sizeof(tbl[0])))
		++blockcount;

	/* pre allocate all the stuff */
	f = calloc(1, sizeof(*f) + super->block_size);
	if (f == NULL)
		goto fail_rd;

	tbl = malloc(count * sizeof(tbl[0]));
	if (tbl == NULL)
		goto fail_rd;

	locations = malloc(blockcount * sizeof(locations[0]));
	if (locations == NULL)
		goto fail_rd;

	/* read the meta block offset table */
	if (lseek(fd, super->fragment_table_start, SEEK_SET) == (off_t)-1)
		goto fail_seek;

	ret = read_retry(fd, locations, blockcount * sizeof(locations[0]));
	if (ret < 0)
		goto fail_rd;

	if ((size_t)ret < (blockcount * sizeof(locations[0])))
		goto fail_trunc;

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
fail_seek:
	perror("seek to fragment table");
	goto fail;
fail_trunc:
	fputs("reading fragment table: unexpected end of file\n", stderr);
	goto fail;
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
