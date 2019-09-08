/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * io.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


typedef struct {
	sqfs_file_t base;

	uint64_t size;
	int fd;
} sqfs_file_stdio_t;


static void stdio_destroy(sqfs_file_t *base)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	close(file->fd);
	free(file);
}

static int stdio_read_at(sqfs_file_t *base, uint64_t offset,
			 void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	ssize_t ret;

	while (size > 0) {
		ret = pread(file->fd, buffer, size, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return SQFS_ERROR_IO;
		}

		if (ret == 0)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		buffer = (char *)buffer + ret;
		size -= ret;
		offset += ret;
	}

	return 0;
}

static int stdio_write_at(sqfs_file_t *base, uint64_t offset,
			  const void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	ssize_t ret;

	while (size > 0) {
		ret = pwrite(file->fd, buffer, size, offset);

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return SQFS_ERROR_IO;
		}

		if (ret == 0)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		buffer = (const char *)buffer + ret;
		size -= ret;
		offset += ret;
	}

	if (offset >= file->size)
		file->size = offset;

	return 0;
}

static uint64_t stdio_get_size(sqfs_file_t *base)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	return file->size;
}

static int stdio_truncate(sqfs_file_t *base, uint64_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	if (ftruncate(file->fd, size))
		return SQFS_ERROR_IO;

	file->size = size;
	return 0;
}


sqfs_file_t *sqfs_open_file(const char *filename, int flags)
{
	sqfs_file_stdio_t *file;
	int open_mode, temp;
	sqfs_file_t *base;

	if (flags & ~SQFS_FILE_OPEN_ALL_FLAGS) {
		errno = EINVAL;
		return NULL;
	}

	file = calloc(1, sizeof(*file));
	base = (sqfs_file_t *)file;
	if (file == NULL)
		return NULL;

	if (flags & SQFS_FILE_OPEN_READ_ONLY) {
		open_mode = O_RDONLY;
	} else {
		open_mode = O_CREAT | O_RDWR;

		if (flags & SQFS_FILE_OPEN_OVERWRITE) {
			open_mode |= O_TRUNC;
		} else {
			open_mode |= O_EXCL;
		}
	}

	file->fd = open(filename, open_mode, 0600);
	if (file->fd < 0) {
		temp = errno;
		free(file);
		errno = temp;
		return NULL;
	}

	base->destroy = stdio_destroy;
	base->read_at = stdio_read_at;
	base->write_at = stdio_write_at;
	base->get_size = stdio_get_size;
	base->truncate = stdio_truncate;
	return base;
}
