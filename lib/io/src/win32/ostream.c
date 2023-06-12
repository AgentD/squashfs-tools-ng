/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"
#include "sqfs/io.h"

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
		if (!WriteFile(file->hnd, data, size, &diff, NULL)) {
			w32_perror(file->path);
			return -1;
		}

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

	if (file->sparse_count == 0)
		return 0;

	if (file->flags & SQFS_FILE_OPEN_NO_SPARSE) {
		bufsz = file->sparse_count > 1024 ? 1024 : file->sparse_count;
		buffer = calloc(1, bufsz);

		if (buffer == NULL) {
			fputs("out-of-memory\n", stderr);
			return -1;
		}

		while (file->sparse_count > 0) {
			diff = file->sparse_count > bufsz ?
				bufsz : file->sparse_count;

			if (write_data(file, buffer, diff)) {
				free(buffer);
				return -1;
			}

			file->sparse_count -= diff;
		}

		free(buffer);
	} else {
		pos.QuadPart = file->sparse_count;

		if (!SetFilePointerEx(file->hnd, pos, NULL, FILE_CURRENT))
			goto fail;

		if (!SetEndOfFile(file->hnd))
			goto fail;

		file->sparse_count = 0;
	}

	return 0;
fail:
	w32_perror(file->path);
	return -1;
}

static int file_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	if (size == 0)
		return 0;

	if (data == NULL) {
		file->sparse_count += size;
		return 0;
	}

	if (realize_sparse(file))
		return -1;

	return write_data(file, data, size);
}

static int file_flush(sqfs_ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	if (realize_sparse(file))
		return -1;

	if (!FlushFileBuffers(file->hnd)) {
		w32_perror(file->path);
		return -1;
	}

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
	file_ostream_t *file = (file_ostream_t *)strm;

	return file->path;
}

sqfs_ostream_t *ostream_open_handle(const char *path, HANDLE hnd, int flags)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	sqfs_ostream_t *strm = (sqfs_ostream_t *)file;
	BOOL ret;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	sqfs_object_init(file, file_destroy, NULL);

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	ret = DuplicateHandle(GetCurrentProcess(), hnd,
			      GetCurrentProcess(), &file->hnd,
			      0, FALSE, DUPLICATE_SAME_ACCESS);
	if (!ret) {
		w32_perror(path);
		goto fail_path;
	}

	CloseHandle(hnd);

	file->flags = flags;
	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(file);
	return NULL;
}

sqfs_ostream_t *ostream_open_file(const char *path, int flags)
{
	sqfs_file_handle_t hnd;
	sqfs_ostream_t *out;

	if (sqfs_open_native_file(&hnd, path, flags)) {
		w32_perror(path);
		return NULL;
	}

	if (hnd == INVALID_HANDLE_VALUE) {
		w32_perror(path);
		return NULL;
	}

	out = ostream_open_handle(path, hnd, flags);
	if (out == NULL)
		CloseHandle(hnd);

	return out;
}

sqfs_ostream_t *ostream_open_stdout(void)
{
	HANDLE hnd = GetStdHandle(STD_OUTPUT_HANDLE);

	return ostream_open_handle("stdout", hnd, SQFS_FILE_OPEN_NO_SPARSE);
}
