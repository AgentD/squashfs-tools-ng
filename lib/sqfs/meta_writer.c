/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
#include "squashfs.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

struct meta_writer_t {
	/* A byte offset into the uncompressed data of the current block */
	size_t offset;

	/* The location of the current block in the file */
	size_t block_offset;

	/* The underlying file descriptor to write to */
	int outfd;

	/* A pointer to the compressor to use for compressing the data */
	compressor_t *cmp;

	/* The raw data chunk that data is appended to */
	uint8_t data[SQFS_META_BLOCK_SIZE + 2];

	/* Scratch buffer for compressing data */
	uint8_t scratch[SQFS_META_BLOCK_SIZE + 2];
};

meta_writer_t *meta_writer_create(int fd, compressor_t *cmp)
{
	meta_writer_t *m = calloc(1, sizeof(*m));

	if (m == NULL) {
		perror("creating meta data writer");
		return NULL;
	}

	m->cmp = cmp;
	m->outfd = fd;
	return m;
}

void meta_writer_destroy(meta_writer_t *m)
{
	free(m);
}

int meta_writer_flush(meta_writer_t *m)
{
	ssize_t ret, count;
	void *ptr;

	if (m->offset == 0)
		return 0;

	ret = m->cmp->do_block(m->cmp, m->data + 2, m->offset,
			       m->scratch + 2, sizeof(m->scratch) - 2);
	if (ret < 0)
		return -1;

	if (ret > 0) {
		((uint16_t *)m->scratch)[0] = htole16(ret);
		count = ret + 2;
		ptr = m->scratch;
	} else {
		((uint16_t *)m->data)[0] = htole16(m->offset | 0x8000);
		count = m->offset + 2;
		ptr = m->data;
	}

	ret = write_retry(m->outfd, ptr, count);

	if (ret < 0) {
		perror("writing meta data");
		return -1;
	}

	if (ret < count) {
		fputs("meta data written to file was truncated\n", stderr);
		return -1;
	}

	memset(m->data, 0, sizeof(m->data));
	m->offset = 0;
	m->block_offset += count;
	return 0;
}

int meta_writer_append(meta_writer_t *m, const void *data, size_t size)
{
	size_t diff;

	while (size != 0) {
		diff = sizeof(m->data) - 2 - m->offset;

		if (diff == 0) {
			if (meta_writer_flush(m))
				return -1;
			diff = sizeof(m->data) - 2;
		}

		if (diff > size)
			diff = size;

		memcpy(m->data + 2 + m->offset, data, diff);
		m->offset += diff;
		size -= diff;
		data = (const char *)data + diff;
	}

	if (m->offset == (sizeof(m->data) - 2))
		return meta_writer_flush(m);

	return 0;
}

void meta_writer_get_position(const meta_writer_t *m, uint64_t *block_start,
			      uint32_t *offset)
{
	*block_start = m->block_offset;
	*offset = m->offset;
}

void meta_writer_reset(meta_writer_t *m)
{
	m->block_offset = 0;
	m->offset = 0;
}
