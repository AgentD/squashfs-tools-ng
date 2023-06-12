/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
	sqfs_istream_t base;
	char *path;
	HANDLE hnd;

	bool eof;
	size_t buffer_offset;
	size_t buffer_used;
	sqfs_u8 buffer[BUFSZ];
} file_istream_t;

static int precache(sqfs_istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;
	DWORD diff, actual;

	if (file->eof)
		return 0;

	if (file->buffer_offset > 0 &&
	    file->buffer_offset < file->buffer_used) {
		memmove(file->buffer, file->buffer + file->buffer_offset,
			file->buffer_used - file->buffer_offset);
	}

	file->buffer_used -= file->buffer_offset;
	file->buffer_offset = 0;

	while (file->buffer_used < sizeof(file->buffer)) {
		diff = sizeof(file->buffer) - file->buffer_used;

		if (!ReadFile(file->hnd, file->buffer + file->buffer_used,
			      diff, &actual, NULL)) {
			DWORD error = GetLastError();

			if (error == ERROR_HANDLE_EOF ||
			    error == ERROR_BROKEN_PIPE) {
				file->eof = true;
				break;
			}

			SetLastError(error);
			w32_perror(file->path);
			return -1;
		}

		if (actual == 0) {
			file->eof = true;
			break;
		}

		file->buffer_used += actual;
	}

	return 0;
}

static int file_get_buffered_data(sqfs_istream_t *strm, const sqfs_u8 **out,
				  size_t *size, size_t want)
{
	file_istream_t *file = (file_istream_t *)strm;

	if (want > BUFSZ)
		want = BUFSZ;

	if (file->buffer_used == 0 ||
	    (file->buffer_used - file->buffer_offset) < want) {
		int ret = precache(strm);
		if (ret)
			return ret;
	}

	*out = file->buffer + file->buffer_offset;
	*size = file->buffer_used - file->buffer_offset;
	return (file->eof && *size == 0) ? 1 : 0;
}

static void file_advance_buffer(sqfs_istream_t *strm, size_t count)
{
	file_istream_t *file = (file_istream_t *)strm;

	assert(count <= file->buffer_used);

	file->buffer_offset += count;

	assert(file->buffer_offset <= file->buffer_used);
}

static const char *file_get_filename(sqfs_istream_t *strm)
{
	return ((file_istream_t *)strm)->path;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_istream_t *file = (file_istream_t *)obj;

	CloseHandle(file->hnd);
	free(file->path);
	free(file);
}

sqfs_istream_t *istream_open_handle(const char *path, HANDLE hnd)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	sqfs_istream_t *strm = (sqfs_istream_t *)file;
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

	strm->get_buffered_data = file_get_buffered_data;
	strm->advance_buffer = file_advance_buffer;
	strm->get_filename = file_get_filename;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(file);
	return NULL;
}

sqfs_istream_t *istream_open_file(const char *path)
{
	WCHAR *wpath = NULL;
	sqfs_istream_t *out;
	HANDLE hnd;

	wpath = path_to_windows(path);
	if (wpath == NULL)
		return NULL;

	hnd = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL,
			  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hnd == INVALID_HANDLE_VALUE) {
		w32_perror(path);
		free(wpath);
		return NULL;
	}

	free(wpath);

	out = istream_open_handle(path, hnd);
	if (out == NULL)
		CloseHandle(hnd);

	return out;
}

sqfs_istream_t *istream_open_stdin(void)
{
	HANDLE hnd = GetStdHandle(STD_INPUT_HANDLE);

	return istream_open_handle("stdin", hnd);
}
