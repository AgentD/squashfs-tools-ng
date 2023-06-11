/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
	ostream_t base;
	char *path;
	HANDLE hnd;
} file_ostream_t;

static int file_append(ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;
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

static int file_append_sparse(ostream_t *strm, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	LARGE_INTEGER pos;

	pos.QuadPart = size;

	if (!SetFilePointerEx(file->hnd, pos, NULL, FILE_CURRENT))
		goto fail;

	if (!SetEndOfFile(file->hnd))
		goto fail;

	return 0;
fail:
	w32_perror(file->path);
	return -1;
}

static int file_flush(ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

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

static const char *file_get_filename(ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	return file->path;
}

ostream_t *ostream_open_handle(const char *path, HANDLE hnd, int flags)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	ostream_t *strm = (ostream_t *)file;
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

	if (flags & OSTREAM_OPEN_SPARSE)
		strm->append_sparse = file_append_sparse;

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

ostream_t *ostream_open_file(const char *path, int flags)
{
	WCHAR *wpath = path_to_windows(path);
	int access_flags, creation_mode;
	ostream_t *out;
	HANDLE hnd;

	if (wpath == NULL)
		return NULL;

	access_flags = GENERIC_WRITE;

	if (flags & OSTREAM_OPEN_OVERWRITE) {
		creation_mode = CREATE_ALWAYS;
	} else {
		creation_mode = CREATE_NEW;
	}

	hnd = CreateFileW(wpath, access_flags, 0, NULL, creation_mode,
			  FILE_ATTRIBUTE_NORMAL, NULL);

	if (hnd == INVALID_HANDLE_VALUE) {
		w32_perror(path);
		free(wpath);
		return NULL;
	}

	free(wpath);

	out = ostream_open_handle(path, hnd, flags);
	if (out == NULL)
		CloseHandle(hnd);

	return out;
}

ostream_t *ostream_open_stdout(void)
{
	HANDLE hnd = GetStdHandle(STD_OUTPUT_HANDLE);

	return ostream_open_handle("stdout", hnd, 0);
}
