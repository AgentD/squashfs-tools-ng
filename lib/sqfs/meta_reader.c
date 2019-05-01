/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_reader.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

meta_reader_t *meta_reader_create(int fd, compressor_t *cmp)
{
	meta_reader_t *m = calloc(1, sizeof(*m));

	if (m == NULL) {
		perror("creating meta data reader");
		return NULL;
	}

	m->fd = fd;
	m->cmp = cmp;
	return m;
}

void meta_reader_destroy(meta_reader_t *m)
{
	free(m);
}

int meta_reader_seek(meta_reader_t *m, uint64_t block_start, size_t offset)
{
	bool compressed;
	uint16_t header;
	ssize_t ret;
	size_t size;

	if (offset >= sizeof(m->data)) {
		fputs("Tried to seek past end of meta data block.\n", stderr);
		return -1;
	}

	if (block_start == m->block_offset) {
		m->offset = offset;
		return 0;
	}

	if (lseek(m->fd, block_start, SEEK_SET) == (off_t)-1) {
		perror("seek on image file");
		return -1;
	}

	ret = read_retry(m->fd, &header, 2);
	if (ret < 0)
		goto fail_rd;

	if ((size_t)ret < 2)
		goto fail_trunc;

	header = le16toh(header);
	compressed = (header & 0x8000) == 0;
	size = header & 0x7FFF;

	if (size > sizeof(m->data)) {
		fputs("found meta data block larger than maximum size\n",
		      stderr);
		return -1;
	}

	m->block_offset = block_start;
	m->offset = offset;
	m->next_block = block_start + size + 2;

	memset(m->data, 0, sizeof(m->data));

	ret = read_retry(m->fd, m->data, size);
	if (ret < 0)
		goto fail_rd;

	if ((size_t)ret < size)
		goto fail_trunc;

	if (compressed) {
		ret = m->cmp->do_block(m->cmp, m->data, size);

		if (ret <= 0) {
			fputs("error uncompressing meta data block\n", stderr);
			return -1;
		}
	}

	return 0;
fail_trunc:
	fputs("reading meta data: unexpected end of file\n", stderr);
	return -1;
fail_rd:
	perror("reading meta data");
	return -1;
}

int meta_reader_read(meta_reader_t *m, void *data, size_t size)
{
	size_t diff;

	while (size != 0) {
		diff = sizeof(m->data) - m->offset;

		if (diff == 0) {
			if (meta_reader_seek(m, m->next_block, 0))
				return -1;
			diff = sizeof(m->data);
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
