/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WINDOWS__)
static int get_file_size(HANDLE fd, sqfs_u64 *out)
{
	LARGE_INTEGER size;
	if (!GetFileSizeEx(fd, &size))
		return -1;
	*out = size.QuadPart;
	return 0;
}
#else
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static int get_file_size(int fd, sqfs_u64 *out)
{
	struct stat sb;
	if (fstat(fd, &sb))
		return -1;
	*out = sb.st_size;
	return 0;
}
#endif

typedef struct {
	sqfs_file_t base;

	bool readonly;
	sqfs_u64 size;
	sqfs_file_handle_t fd;

	char name[];
} sqfs_file_stdio_t;


static void stdio_destroy(sqfs_object_t *base)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	sqfs_close_native_file(file->fd);
	free(file);
}

static sqfs_object_t *stdio_copy(const sqfs_object_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;
	sqfs_file_stdio_t *copy;
	size_t size;

	if (!file->readonly) {
#if defined(_WIN32) || defined(__WINDOWS__)
		SetLastError(ERROR_NOT_SUPPORTED);
#else
		errno = ENOTSUP;
#endif
		return NULL;
	}

	size = sizeof(*file) + strlen(file->name) + 1;
	copy = calloc(1, size);
	if (copy == NULL)
		return NULL;

	memcpy(copy, file, size);

	if (sqfs_duplicate_native_file(file->fd, &copy->fd)) {
		os_error_t error = get_os_error_state();
		free(copy);
		copy = NULL;
		set_os_error_state(error);
	}

	return (sqfs_object_t *)copy;
}

#if defined(_WIN32) || defined(__WINDOWS__)
static int stdio_read_at(sqfs_file_t *base, sqfs_u64 offset,
			 void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	DWORD actually_read;
	int ret;

	if (offset >= file->size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (size == 0)
		return 0;

	if ((offset + size - 1) >= file->size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	ret = sqfs_seek_native_file(file->fd, offset, SQFS_FILE_SEEK_START);
	if (ret)
		return ret;

	while (size > 0) {
		if (!ReadFile(file->fd, buffer, size, &actually_read, NULL))
			return SQFS_ERROR_IO;

		size -= actually_read;
		buffer = (char *)buffer + actually_read;
	}

	return 0;
}

static int stdio_write_at(sqfs_file_t *base, sqfs_u64 offset,
			  const void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	DWORD actually_read;
	int ret;

	if (size == 0)
		return 0;

	ret = sqfs_seek_native_file(file->fd, offset, SQFS_FILE_SEEK_START);
	if (ret)
		return ret;

	while (size > 0) {
		if (!WriteFile(file->fd, buffer, size, &actually_read, NULL))
			return SQFS_ERROR_IO;

		size -= actually_read;
		buffer = (char *)buffer + actually_read;
		offset += actually_read;

		if (offset > file->size)
			file->size = offset;
	}

	return 0;
}
#else
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
#endif

static sqfs_u64 stdio_get_size(const sqfs_file_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;

	return file->size;
}

static int stdio_truncate(sqfs_file_t *base, sqfs_u64 size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	int ret;

	ret = sqfs_seek_native_file(file->fd, size, SQFS_FILE_SEEK_START |
				    SQFS_FILE_SEEK_TRUNCATE);
	if (ret)
		return ret;

	file->size = size;
	return 0;
}

static const char *stdio_get_filename(sqfs_file_t *file)
{
	return ((sqfs_file_stdio_t *)file)->name;
}

sqfs_file_t *sqfs_open_file(const char *filename, sqfs_u32 flags)
{
	bool file_opened = false;
	sqfs_file_stdio_t *file;
	size_t size, namelen;
	sqfs_file_t *base;
	os_error_t err;

	namelen = strlen(filename);
	size = sizeof(*file) + 1;

	if (SZ_ADD_OV(size, namelen, &size))
		goto fail_no_mem;

	file = calloc(1, size);
	base = (sqfs_file_t *)file;
	if (file == NULL)
		goto fail_no_mem;

	sqfs_object_init(file, stdio_destroy, stdio_copy);
	memcpy(file->name, filename, namelen);

	if (sqfs_open_native_file(&file->fd, filename, flags))
		goto fail;

	file_opened = true;
	if (get_file_size(file->fd, &file->size))
		goto fail;

	file->readonly = (flags & SQFS_FILE_OPEN_READ_ONLY) != 0;

	base->read_at = stdio_read_at;
	base->write_at = stdio_write_at;
	base->get_size = stdio_get_size;
	base->truncate = stdio_truncate;
	base->get_filename = stdio_get_filename;
	return base;
fail:
	err = get_os_error_state();
	if (file_opened)
		sqfs_close_native_file(file->fd);
	free(file);
	set_os_error_state(err);
	return NULL;
fail_no_mem:
#if defined(_WIN32) || defined(__WINDOWS__)
	SetLastError(ERROR_NOT_ENOUGH_MEMORY);
#else
	errno = ENOMEM;
#endif
	return NULL;
}
