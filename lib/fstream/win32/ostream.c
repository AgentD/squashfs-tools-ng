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

static int w32_append(HANDLE hnd, const char *filename,
		      const void *data, size_t size)
{
	DWORD diff;

	while (size > 0) {
		if (!WriteFile(hnd, data, size, &diff, NULL)) {
			w32_perror(filename);
			return -1;
		}

		size -= diff;
		data = (const char *)data + diff;
	}

	return 0;
}

static int w32_flush(HANDLE hnd, const char *filename)
{
	if (FlushFileBuffers(hnd) != 0) {
		w32_perror(filename);
		return -1;
	}

	return 0;
}

/*****************************************************************************/

static int file_append(ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	return w32_append(file->hnd, file->path, data, size);
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

	return w32_flush(file->hnd, file->path);
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

/*****************************************************************************/

static int stdout_append(ostream_t *strm, const void *data, size_t size)
{
	(void)strm;
	return w32_append(GetStdHandle(STD_OUTPUT_HANDLE), "stdout",
			  data, size);
}

static int stdout_flush(ostream_t *strm)
{
	(void)strm;
	return w32_flush(GetStdHandle(STD_OUTPUT_HANDLE), "stdout");
}

static void stdout_destroy(sqfs_object_t *obj)
{
	free(obj);
}

static const char *stdout_get_filename(ostream_t *strm)
{
	(void)strm;
	return "stdout";
}

/*****************************************************************************/

ostream_t *ostream_open_file(const char *path, int flags)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	sqfs_object_t *obj = (sqfs_object_t *)file;
	ostream_t *strm = (ostream_t *)file;
	int access_flags, creation_mode;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	access_flags = GENERIC_WRITE;

	if (flags & OSTREAM_OPEN_OVERWRITE) {
		creation_mode = CREATE_ALWAYS;
	} else {
		creation_mode = CREATE_NEW;
	}

	file->hnd = CreateFile(path, access_flags, 0, NULL, creation_mode,
			       FILE_ATTRIBUTE_NORMAL, NULL);

	if (file->hnd == INVALID_HANDLE_VALUE) {
		w32_perror(path);
		goto fail_path;
	}

	if (flags & OSTREAM_OPEN_SPARSE)
		strm->append_sparse = file_append_sparse;

	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;
	obj->destroy = file_destroy;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(file);
	return NULL;
}

ostream_t *ostream_open_stdout(void)
{
	ostream_t *strm = calloc(1, sizeof(strm));
	sqfs_object_t *obj = (sqfs_object_t *)strm;

	if (strm == NULL) {
		perror("creating stdout file wrapper");
		return NULL;
	}

	strm->append = stdout_append;
	strm->flush = stdout_flush;
	strm->get_filename = stdout_get_filename;
	obj->destroy = stdout_destroy;
	return strm;
}
