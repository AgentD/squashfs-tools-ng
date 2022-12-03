/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * io_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>


typedef struct {
	sqfs_file_t base;

	bool readonly;
	sqfs_u64 size;
	int fd;
} sqfs_file_stdio_t;


static void stdio_destroy(sqfs_object_t *base)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	close(file->fd);
	free(file);
}

static sqfs_object_t *stdio_copy(const sqfs_object_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;
	sqfs_file_stdio_t *copy;
	int err;

	if (!file->readonly) {
		errno = ENOTSUP;
		return NULL;
	}

	copy = calloc(1, sizeof(*copy));
	if (copy == NULL)
		return NULL;

	memcpy(copy, file, sizeof(*file));

	copy->fd = dup(file->fd);
	if (copy->fd < 0) {
		err = errno;
		free(copy);
		copy = NULL;
		errno = err;
	}

	return (sqfs_object_t *)copy;
}

static int stdio_read_at(sqfs_file_t *base, sqfs_u64 offset,
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

static int stdio_write_at(sqfs_file_t *base, sqfs_u64 offset,
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

static sqfs_u64 stdio_get_size(const sqfs_file_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;

	return file->size;
}

static int stdio_truncate(sqfs_file_t *base, sqfs_u64 size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	if (ftruncate(file->fd, size))
		return SQFS_ERROR_IO;

	file->size = size;
	return 0;
}


sqfs_file_t *sqfs_open_file(const char *filename, sqfs_u32 flags)
{
	sqfs_file_stdio_t *file;
	int open_mode, temp;
	sqfs_file_t *base;
	struct stat sb;

	if (flags & ~SQFS_FILE_OPEN_ALL_FLAGS) {
		errno = EINVAL;
		return NULL;
	}

	file = calloc(1, sizeof(*file));
	base = (sqfs_file_t *)file;
	if (file == NULL)
		return NULL;

	sqfs_object_init(file, stdio_destroy, stdio_copy);

	if (flags & SQFS_FILE_OPEN_READ_ONLY) {
		file->readonly = true;
		open_mode = O_RDONLY;
	} else {
		file->readonly = false;
		open_mode = O_CREAT | O_RDWR;

		if (flags & SQFS_FILE_OPEN_OVERWRITE) {
			open_mode |= O_TRUNC;
		} else {
			open_mode |= O_EXCL;
		}
	}

	file->fd = open(filename, open_mode, 0644);
	if (file->fd < 0) {
		temp = errno;
		free(file);
		errno = temp;
		return NULL;
	}

	if (fstat(file->fd, &sb)) {
		temp = errno;
		close(file->fd);
		free(file);
		errno = temp;
		return NULL;
	}

	file->size = sb.st_size;

	base->read_at = stdio_read_at;
	base->write_at = stdio_write_at;
	base->get_size = stdio_get_size;
	base->truncate = stdio_truncate;
	return base;
}
