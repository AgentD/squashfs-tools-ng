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

	hnd = file->path == NULL ? GetStdHandle(STD_INPUT_HANDLE) : file->hnd;

	while (strm->buffer_used < sizeof(file->buffer)) {
		diff = sizeof(file->buffer) - strm->buffer_used;

		if (!ReadFile(hnd, strm->buffer + strm->buffer_used,
			      diff, &actual, NULL)) {
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
	sqfs_object_t *obj = (sqfs_object_t *)file;
	istream_t *strm = (istream_t *)file;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	file->hnd = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
			       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file->hnd == INVALID_HANDLE_VALUE) {
		perror(path);
		goto fail_path;
	}

	strm->buffer = file->buffer;
	strm->precache = file_precache;
	strm->get_filename = file_get_filename;
	obj->destroy = file_destroy;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(file);
	return NULL;
}

istream_t *istream_open_stdin(void)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	sqfs_object_t *obj = (sqfs_object_t *)file;
	istream_t *strm = (istream_t *)file;

	if (file == NULL) {
		perror("stdin");
		return NULL;
	}

	strm->buffer = file->buffer;
	strm->precache = file_precache;
	strm->get_filename = file_get_filename;
	obj->destroy = file_destroy;
	return strm;
}
