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
	istream_t base;
	char *path;
	HANDLE hnd;

	sqfs_u8 buffer[BUFSZ];
} file_istream_t;

static int file_precache(istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;
	DWORD diff, actual;
	HANDLE hnd;

	if (strm->eof)
		return 0;

	assert(strm->buffer >= file->buffer);
	assert(strm->buffer <= (file->buffer + BUFSZ));
	assert(strm->buffer_used <= BUFSZ);
	assert((size_t)(strm->buffer - file->buffer) <=
	       (BUFSZ - strm->buffer_used));

	if (strm->buffer_used > 0)
		memmove(file->buffer, strm->buffer, strm->buffer_used);

	strm->buffer = file->buffer;
	hnd = file->path == NULL ? GetStdHandle(STD_INPUT_HANDLE) : file->hnd;

	while (strm->buffer_used < sizeof(file->buffer)) {
		diff = sizeof(file->buffer) - strm->buffer_used;

		if (!ReadFile(hnd, file->buffer + strm->buffer_used,
			      diff, &actual, NULL)) {
			DWORD error = GetLastError();

			if (error == ERROR_HANDLE_EOF ||
			    error == ERROR_BROKEN_PIPE) {
				strm->eof = true;
				break;
			}

			SetLastError(error);

			w32_perror(file->path == NULL ? "stdin" : file->path);
			return -1;
		}

		if (actual == 0) {
			strm->eof = true;
			break;
		}

		strm->buffer_used += actual;
	}

	return 0;
}

static const char *file_get_filename(istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;

	return file->path == NULL ? "stdin" : file->path;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_istream_t *file = (file_istream_t *)obj;

	if (file->path != NULL) {
		CloseHandle(file->hnd);
		free(file->path);
	}

	free(file);
}

istream_t *istream_open_file(const char *path)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	istream_t *strm = (istream_t *)file;
	WCHAR *wpath = NULL;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	sqfs_object_init(file, file_destroy, NULL);

	wpath = path_to_windows(path);
	if (wpath == NULL)
		goto fail_free;

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	file->hnd = CreateFileW(wpath, GENERIC_READ, FILE_SHARE_READ, NULL,
			       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file->hnd == INVALID_HANDLE_VALUE) {
		perror(path);
		goto fail_path;
	}

	free(wpath);

	strm->buffer = file->buffer;
	strm->precache = file_precache;
	strm->get_filename = file_get_filename;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(wpath);
	free(file);
	return NULL;
}

istream_t *istream_open_stdin(void)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	istream_t *strm = (istream_t *)file;

	if (file == NULL) {
		perror("stdin");
		return NULL;
	}

	sqfs_object_init(file, file_destroy, NULL);

	strm->buffer = file->buffer;
	strm->precache = file_precache;
	strm->get_filename = file_get_filename;
	return strm;
}
