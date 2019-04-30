/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "mksquashfs.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

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

	if (m->offset == 0)
		return 0;

	ret = m->cmp->do_block(m->cmp, m->data + 2, m->offset);
	if (ret < 0)
		return -1;

	if (ret > 0) {
		((uint16_t *)m->data)[0] = htole16(ret);
		count = ret + 2;
	} else {
		((uint16_t *)m->data)[0] = htole16(m->offset | 0x8000);
		count = m->offset + 2;
	}

	ret = write_retry(m->outfd, m->data, count);

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
