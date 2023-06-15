/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
	sqfs_ostream_t base;
	sqfs_u64 sparse_count;
	char *path;
	HANDLE hnd;
	int flags;
} file_ostream_t;

static int write_data(file_ostream_t *file, const void *data, size_t size)
{
	DWORD diff;

	while (size > 0) {
		if (!WriteFile(file->hnd, data, size, &diff, NULL))
			return SQFS_ERROR_IO;

		size -= diff;
		data = (const char *)data + diff;
	}

	return 0;
}

static int realize_sparse(file_ostream_t *file)
{
	size_t bufsz, diff;
	LARGE_INTEGER pos;
	void *buffer;
	int ret;

	if (file->sparse_count == 0)
		return 0;

	if (file->flags & SQFS_FILE_OPEN_NO_SPARSE) {
		bufsz = file->sparse_count > 1024 ? 1024 : file->sparse_count;
		buffer = calloc(1, bufsz);

		if (buffer == NULL)
			return SQFS_ERROR_ALLOC;

		while (file->sparse_count > 0) {
			diff = file->sparse_count > bufsz ?
				bufsz : file->sparse_count;

			ret = write_data(file, buffer, diff);
			if (ret) {
				os_error_t err = get_os_error_state();
				free(buffer);
				set_os_error_state(err);
				return ret;
			}

			file->sparse_count -= diff;
		}

		free(buffer);
	} else {
		pos.QuadPart = file->sparse_count;

		if (!SetFilePointerEx(file->hnd, pos, NULL, FILE_CURRENT))
			return SQFS_ERROR_IO;

		if (!SetEndOfFile(file->hnd))
			return SQFS_ERROR_IO;

		file->sparse_count = 0;
	}

	return 0;
}

static int file_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	int ret;

	if (size == 0 || data == NULL) {
		file->sparse_count += size;
		return 0;
	}

	ret = realize_sparse(file);
	if (ret)
		return ret;

	return write_data(file, data, size);
}

static int file_flush(sqfs_ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	int ret;

	ret = realize_sparse(file);
	if (ret)
		return ret;

	if (!FlushFileBuffers(file->hnd))
		return SQFS_ERROR_IO;

	return 0;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_ostream_t *file = (file_ostream_t *)obj;

	CloseHandle(file->hnd);
	free(file->path);
	free(file);
}

static const char *file_get_filename(sqfs_ostream_t *strm)
{
	return ((file_ostream_t *)strm)->path;
}

int sqfs_ostream_open_handle(sqfs_ostream_t **out, const char *path,
			     sqfs_file_handle_t hnd, sqfs_u32 flags)
{
	file_ostream_t *file;
	sqfs_ostream_t *strm;
	BOOL ret;

	if (flags & ~(SQFS_FILE_OPEN_ALL_FLAGS))
		return SQFS_ERROR_UNSUPPORTED;

	file = calloc(1, sizeof(*file));
	strm = (sqfs_ostream_t *)file;
	if (file == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(file, file_destroy, NULL);

	file->path = strdup(path);
	if (file->path == NULL) {
		free(file);
		return SQFS_ERROR_ALLOC;
	}

	ret = DuplicateHandle(GetCurrentProcess(), hnd,
			      GetCurrentProcess(), &file->hnd,
			      0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!ret) {
		os_error_t err = get_os_error_state();
		free(file->path);
		free(file);
		set_os_error_state(err);
		return SQFS_ERROR_IO;
	}

	CloseHandle(hnd);

	file->flags = flags;
	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;

	*out = strm;
	return 0;
}

int sqfs_ostream_open_file(sqfs_ostream_t **out, const char *path,
			   sqfs_u32 flags)
{
	sqfs_file_handle_t hnd;
	int ret;

	*out = NULL;
	if (flags & SQFS_FILE_OPEN_READ_ONLY)
		return SQFS_ERROR_ARG_INVALID;

	ret = sqfs_open_native_file(&hnd, path, flags);
	if (ret)
		return ret;

	ret = sqfs_ostream_open_handle(out, path, hnd, flags);
	if (ret) {
		os_error_t err = get_os_error_state();
		CloseHandle(hnd);
		set_os_error_state(err);
		return SQFS_ERROR_IO;
	}

	return 0;
}
