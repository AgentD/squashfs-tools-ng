/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"

#define BUFSZ (131072)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>

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
			os_error_t error = get_os_error_state();

			if (error.w32_errno == ERROR_HANDLE_EOF ||
			    error.w32_errno == ERROR_BROKEN_PIPE) {
				file->eof = true;
				break;
			}

			set_os_error_state(error);
			return SQFS_ERROR_IO;
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

int sqfs_istream_open_handle(sqfs_istream_t **out, const char *path,
			     sqfs_file_handle_t hnd, sqfs_u32 flags)
{
	file_istream_t *file;
	sqfs_istream_t *strm;
	BOOL ret;

	if (flags & ~(SQFS_FILE_OPEN_ALL_FLAGS))
		return SQFS_ERROR_UNSUPPORTED;

	file = calloc(1, sizeof(*file));
	strm = (sqfs_istream_t *)file;
	if (file == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(file, file_destroy, NULL);

	file->path = strdup(path);
	if (file->path == NULL) {
		os_error_t err = get_os_error_state();
		free(file);
		set_os_error_state(err);
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

	strm->get_buffered_data = file_get_buffered_data;
	strm->advance_buffer = file_advance_buffer;
	strm->get_filename = file_get_filename;

	*out = strm;
	return 0;
}

int sqfs_istream_open_file(sqfs_istream_t **out, const char *path,
			   sqfs_u32 flags)
{
	sqfs_file_handle_t hnd;
	int ret;

	flags |= SQFS_FILE_OPEN_READ_ONLY;

	if (flags & (SQFS_FILE_OPEN_OVERWRITE | SQFS_FILE_OPEN_NO_SPARSE))
		return SQFS_ERROR_ARG_INVALID;

	ret = sqfs_open_native_file(&hnd, path, flags);
	if (ret)
		return ret;

	ret = sqfs_istream_open_handle(out, path, hnd, flags);
	if (ret) {
		os_error_t err = get_os_error_state();
		CloseHandle(hnd);
		set_os_error_state(err);
		return ret;
	}

	return 0;
}
