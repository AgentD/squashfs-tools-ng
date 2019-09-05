/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * meta_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/data.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

typedef struct meta_block_t {
	struct meta_block_t *next;

	/* possibly compressed data with 2 byte header */
	uint8_t data[SQFS_META_BLOCK_SIZE + 2];
} meta_block_t;

struct sqfs_meta_writer_t {
	/* A byte offset into the uncompressed data of the current block */
	size_t offset;

	/* The location of the current block in the file */
	size_t block_offset;

	/* The underlying file descriptor to write to */
	int outfd;

	/* A pointer to the compressor to use for compressing the data */
	sqfs_compressor_t *cmp;

	/* The raw data chunk that data is appended to */
	uint8_t data[SQFS_META_BLOCK_SIZE];

	bool keep_in_mem;
	meta_block_t *list;
	meta_block_t *list_end;
};

static int write_block(int fd, meta_block_t *outblk)
{
	size_t count = le16toh(((uint16_t *)outblk->data)[0]) & 0x7FFF;

	return write_data("writing meta data block", fd,
			  outblk->data, count + 2);
}

sqfs_meta_writer_t *sqfs_meta_writer_create(int fd, sqfs_compressor_t *cmp,
					    bool keep_in_mem)
{
	sqfs_meta_writer_t *m = calloc(1, sizeof(*m));

	if (m == NULL) {
		perror("creating meta data writer");
		return NULL;
	}

	m->cmp = cmp;
	m->outfd = fd;
	m->keep_in_mem = keep_in_mem;
	return m;
}

void sqfs_meta_writer_destroy(sqfs_meta_writer_t *m)
{
	meta_block_t *blk;

	while (m->list != NULL) {
		blk = m->list;
		m->list = blk->next;
		free(blk);
	}

	free(m);
}

int sqfs_meta_writer_flush(sqfs_meta_writer_t *m)
{
	meta_block_t *outblk;
	size_t count;
	ssize_t ret;

	if (m->offset == 0)
		return 0;

	outblk = calloc(1, sizeof(*outblk));
	if (outblk == NULL) {
		perror("generating meta data block");
		return -1;
	}

	ret = m->cmp->do_block(m->cmp, m->data, m->offset,
			       outblk->data + 2, sizeof(outblk->data) - 2);
	if (ret < 0) {
		free(outblk);
		return -1;
	}

	if (ret > 0) {
		((uint16_t *)outblk->data)[0] = htole16(ret);
		count = ret + 2;
	} else {
		((uint16_t *)outblk->data)[0] = htole16(m->offset | 0x8000);
		memcpy(outblk->data + 2, m->data, m->offset);
		count = m->offset + 2;
	}

	if (m->keep_in_mem) {
		if (m->list == NULL) {
			m->list = outblk;
		} else {
			m->list_end->next = outblk;
		}
		m->list_end = outblk;
		ret = 0;
	} else {
		ret = write_block(m->outfd, outblk);
		free(outblk);
	}

	memset(m->data, 0, sizeof(m->data));
	m->offset = 0;
	m->block_offset += count;
	return ret;
}

int sqfs_meta_writer_append(sqfs_meta_writer_t *m, const void *data,
			    size_t size)
{
	size_t diff;

	while (size != 0) {
		diff = sizeof(m->data) - m->offset;

		if (diff == 0) {
			if (sqfs_meta_writer_flush(m))
				return -1;
			diff = sizeof(m->data);
		}

		if (diff > size)
			diff = size;

		memcpy(m->data + m->offset, data, diff);
		m->offset += diff;
		size -= diff;
		data = (const char *)data + diff;
	}

	if (m->offset == sizeof(m->data))
		return sqfs_meta_writer_flush(m);

	return 0;
}

void sqfs_meta_writer_get_position(const sqfs_meta_writer_t *m,
				   uint64_t *block_start,
				   uint32_t *offset)
{
	*block_start = m->block_offset;
	*offset = m->offset;
}

void sqfs_meta_writer_reset(sqfs_meta_writer_t *m)
{
	m->block_offset = 0;
	m->offset = 0;
}

int sqfs_meta_write_write_to_file(sqfs_meta_writer_t *m)
{
	meta_block_t *blk;

	while (m->list != NULL) {
		blk = m->list;

		if (write_block(m->outfd, blk))
			return -1;

		m->list = blk->next;
		free(blk);
	}

	m->list_end = NULL;
	return 0;
}
