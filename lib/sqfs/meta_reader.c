/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * meta_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_reader.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

struct sqfs_meta_reader_t {
	uint64_t start;
	uint64_t limit;
	size_t data_used;

	/* The location of the current block in the image */
	uint64_t block_offset;

	/* The location of the next block after the current one */
	uint64_t next_block;

	/* A byte offset into the uncompressed data of the current block */
	size_t offset;

	/* The underlying file descriptor to read from */
	int fd;

	/* A pointer to the compressor to use for extracting data */
	sqfs_compressor_t *cmp;

	/* The raw data read from the input file */
	uint8_t data[SQFS_META_BLOCK_SIZE];

	/* The uncompressed data read from the input file */
	uint8_t scratch[SQFS_META_BLOCK_SIZE];
};

sqfs_meta_reader_t *sqfs_meta_reader_create(int fd, sqfs_compressor_t *cmp,
					    uint64_t start, uint64_t limit)
{
	sqfs_meta_reader_t *m = calloc(1, sizeof(*m));

	if (m == NULL) {
		perror("creating meta data reader");
		return NULL;
	}

	m->start = start;
	m->limit = limit;
	m->fd = fd;
	m->cmp = cmp;
	return m;
}

void sqfs_meta_reader_destroy(sqfs_meta_reader_t *m)
{
	free(m);
}

int sqfs_meta_reader_seek(sqfs_meta_reader_t *m, uint64_t block_start,
			  size_t offset)
{
	bool compressed;
	uint16_t header;
	ssize_t ret;
	size_t size;

	if (block_start < m->start || block_start >= m->limit)
		goto fail_range;

	if (block_start == m->block_offset) {
		if (offset >= m->data_used)
			goto fail_offset;

		m->offset = offset;
		return 0;
	}

	if (read_data_at("reading meta data header", block_start,
			 m->fd, &header, 2)) {
		return -1;
	}

	header = le16toh(header);
	compressed = (header & 0x8000) == 0;
	size = header & 0x7FFF;

	if (size > sizeof(m->data))
		goto fail_too_large;

	if ((block_start + 2 + size) > m->limit)
		goto fail_block_bounds;

	if (read_data_at("reading meta data block", block_start + 2,
			 m->fd, m->data, size)) {
		return -1;
	}

	if (compressed) {
		ret = m->cmp->do_block(m->cmp, m->data, size,
				       m->scratch, sizeof(m->scratch));

		if (ret <= 0) {
			fputs("error uncompressing meta data block\n", stderr);
			return -1;
		}

		memcpy(m->data, m->scratch, ret);
		m->data_used = ret;
	} else {
		m->data_used = size;
	}

	if (offset >= m->data_used)
		goto fail_offset;

	m->block_offset = block_start;
	m->next_block = block_start + size + 2;
	m->offset = offset;
	return 0;
fail_block_bounds:
	fputs("found metadata block that exceeds filesystem bounds.\n",
	      stderr);
	return -1;
fail_too_large:
	fputs("found metadata block larger than maximum size.\n", stderr);
	return -1;
fail_offset:
	fputs("Tried to seek past end of metadata block.\n", stderr);
	return -1;
fail_range:
	fputs("Tried to read meta data block past filesystem bounds.\n",
	      stderr);
	return -1;
}

void sqfs_meta_reader_get_position(sqfs_meta_reader_t *m, uint64_t *block_start,
				   size_t *offset)
{
	*block_start = m->block_offset;
	*offset = m->offset;
}

int sqfs_meta_reader_read(sqfs_meta_reader_t *m, void *data, size_t size)
{
	size_t diff;

	while (size != 0) {
		diff = m->data_used - m->offset;

		if (diff == 0) {
			if (sqfs_meta_reader_seek(m, m->next_block, 0))
				return -1;
			diff = m->data_used;
		}

		if (diff > size)
			diff = size;

		memcpy(data, m->data + m->offset, diff);

		m->offset += diff;
		data = (char *)data + diff;
		size -= diff;
	}

	return 0;
}
