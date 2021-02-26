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

#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>


typedef struct {
	sqfs_file_t base;

	bool readonly;
	sqfs_u64 size;
	HANDLE fd;
} sqfs_file_stdio_t;


static void stdio_destroy(sqfs_object_t *base)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;

	CloseHandle(file->fd);
	free(file);
}

static sqfs_object_t *stdio_copy(const sqfs_object_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;
	sqfs_file_stdio_t *copy;
	BOOL ret;

	if (!file->readonly) {
		SetLastError(ERROR_NOT_SUPPORTED);
		return NULL;
	}

	copy = calloc(1, sizeof(*copy));
	if (copy == NULL)
		return NULL;

	memcpy(copy, file, sizeof(*file));

	ret = DuplicateHandle(GetCurrentProcess(), file->fd,
			      GetCurrentProcess(), &copy->fd,
			      0, FALSE, DUPLICATE_SAME_ACCESS);

	if (!ret) {
		free(copy);
		return NULL;
	}

	return (sqfs_object_t *)copy;
}

static int stdio_read_at(sqfs_file_t *base, sqfs_u64 offset,
			 void *buffer, size_t size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	DWORD actually_read;
	LARGE_INTEGER pos;

	if (offset >= file->size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (size == 0)
		return 0;

	if ((offset + size - 1) >= file->size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	pos.QuadPart = offset;

	if (!SetFilePointerEx(file->fd, pos, NULL, FILE_BEGIN))
		return SQFS_ERROR_IO;

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
	LARGE_INTEGER pos;

	if (size == 0)
		return 0;

	pos.QuadPart = offset;

	if (!SetFilePointerEx(file->fd, pos, NULL, FILE_BEGIN))
		return SQFS_ERROR_IO;

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

static sqfs_u64 stdio_get_size(const sqfs_file_t *base)
{
	const sqfs_file_stdio_t *file = (const sqfs_file_stdio_t *)base;

	return file->size;
}

static int stdio_truncate(sqfs_file_t *base, sqfs_u64 size)
{
	sqfs_file_stdio_t *file = (sqfs_file_stdio_t *)base;
	LARGE_INTEGER pos;

	pos.QuadPart = size;

	if (!SetFilePointerEx(file->fd, pos, NULL, FILE_BEGIN))
		return SQFS_ERROR_IO;

	if (!SetEndOfFile(file->fd))
		return SQFS_ERROR_IO;

	file->size = size;
	return 0;
}


sqfs_file_t *sqfs_open_file(const char *filename, sqfs_u32 flags)
{
	int access_flags, creation_mode, share_mode;
	sqfs_file_stdio_t *file;
	LARGE_INTEGER size;
	sqfs_file_t *base;

	if (flags & ~SQFS_FILE_OPEN_ALL_FLAGS)
		return NULL;

	file = calloc(1, sizeof(*file));
	base = (sqfs_file_t *)file;
	if (file == NULL)
		return NULL;

	if (flags & SQFS_FILE_OPEN_READ_ONLY) {
		file->readonly = true;
		access_flags = GENERIC_READ;
		creation_mode = OPEN_EXISTING;
		share_mode = FILE_SHARE_READ;
	} else {
		file->readonly = false;
		access_flags = GENERIC_READ | GENERIC_WRITE;
		share_mode = 0;

		if (flags & SQFS_FILE_OPEN_OVERWRITE) {
			creation_mode = CREATE_ALWAYS;
		} else {
			creation_mode = CREATE_NEW;
		}
	}

	file->fd = CreateFile(filename, access_flags, share_mode, NULL, creation_mode,
			      FILE_ATTRIBUTE_NORMAL, NULL);

	if (file->fd == INVALID_HANDLE_VALUE) {
		free(file);
		return NULL;
	}

	if (!GetFileSizeEx(file->fd, &size)) {
		free(file);
		return NULL;
	}

	file->size = size.QuadPart;
	base->read_at = stdio_read_at;
	base->write_at = stdio_write_at;
	base->get_size = stdio_get_size;
	base->truncate = stdio_truncate;
	((sqfs_object_t *)base)->destroy = stdio_destroy;
	((sqfs_object_t *)base)->copy = stdio_copy;
	return base;
}
