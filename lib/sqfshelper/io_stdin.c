/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * io_stdin.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "sqfs/error.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>


typedef struct {
	sqfs_file_t base;

	const sparse_map_t *map;
	uint64_t offset;
	uint64_t size;
} sqfs_file_stdin_t;


static void stdin_destroy(sqfs_file_t *base)
{
	free(base);
}

static int stdin_read_at(sqfs_file_t *base, uint64_t offset,
			 void *buffer, size_t size)
{
	sqfs_file_stdin_t *file = (sqfs_file_stdin_t *)base;
	size_t temp_size = 0;
	uint8_t *temp = NULL;
	uint64_t diff;
	ssize_t ret;

	if (offset < file->offset)
		return SQFS_ERROR_IO;

	if (offset > file->offset) {
		temp_size = 1024;
		temp = alloca(temp_size);
	}

	if (offset >= file->size || (offset + size) > file->size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	while (size > 0) {
		if (offset > file->offset) {
			diff = file->offset - offset;
			diff = diff > (uint64_t)temp_size ? temp_size : diff;

			ret = read(STDIN_FILENO, temp, diff);
		} else {
			ret = read(STDIN_FILENO, buffer, size);
		}

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return SQFS_ERROR_IO;
		}

		if (ret == 0)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		if (offset <= file->offset) {
			buffer = (char *)buffer + ret;
			size -= ret;
			offset += ret;
		}

		file->offset += ret;
	}

	return 0;
}

static int stdin_read_condensed(sqfs_file_t *base, uint64_t offset,
				void *buffer, size_t size)
{
	sqfs_file_stdin_t *file = (sqfs_file_stdin_t *)base;
	uint64_t poffset = 0, src_start;
	size_t dst_start, diff, count;
	const sparse_map_t *it;
	int err;

	memset(buffer, 0, size);

	for (it = file->map; it != NULL; it = it->next) {
		if (it->offset + it->count <= offset) {
			poffset += it->count;
			continue;
		}

		if (it->offset >= offset + size) {
			poffset += it->count;
			continue;
		}

		count = size;

		if (offset + count >= it->offset + it->count)
			count = it->offset + it->count - offset;

		if (it->offset < offset) {
			diff = offset - it->offset;

			src_start = poffset + diff;
			dst_start = 0;
			count -= diff;
		} else if (it->offset > offset) {
			diff = it->offset - offset;

			src_start = poffset;
			dst_start = diff;
		} else {
			src_start = poffset;
			dst_start = 0;
		}

		err = stdin_read_at(base, src_start,
				    (char *)buffer + dst_start, count);
		if (err)
			return err;

		poffset += it->count;
	}

	return 0;
}

static int stdin_write_at(sqfs_file_t *base, uint64_t offset,
			  const void *buffer, size_t size)
{
	(void)base; (void)offset; (void)buffer; (void)size;
	return SQFS_ERROR_IO;
}

static uint64_t stdin_get_size(const sqfs_file_t *base)
{
	return ((const sqfs_file_stdin_t *)base)->size;
}

static int stdin_truncate(sqfs_file_t *base, uint64_t size)
{
	(void)base; (void)size;
	return SQFS_ERROR_IO;
}

sqfs_file_t *sqfs_get_stdin_file(const sparse_map_t *map, uint64_t size)
{
	sqfs_file_stdin_t *file = calloc(1, sizeof(*file));
	sqfs_file_t *base = (sqfs_file_t *)file;

	if (file == NULL)
		return NULL;

	file->size = size;
	file->map = map;

	base->destroy = stdin_destroy;
	base->write_at = stdin_write_at;
	base->get_size = stdin_get_size;
	base->truncate = stdin_truncate;

	if (map == NULL) {
		base->read_at = stdin_read_at;
	} else {
		base->read_at = stdin_read_condensed;
	}
	return base;
}
