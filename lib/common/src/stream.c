/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mem.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"
#include "common.h"
#include "sqfs/io.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#define STD_INPUT_HANDLE STDIN_FILENO
#define STD_OUTPUT_HANDLE STDOUT_FILENO
#define GetStdHandle(hnd) hnd
#endif

typedef struct {
	sqfs_istream_t base;

	sqfs_u8 *buffer;
	size_t bufsz;

	const void *data;
	size_t size;
	size_t offset;
	size_t visible;

	char *name;
} mem_istream_t;

static int mem_get_buffered_data(sqfs_istream_t *strm, const sqfs_u8 **out,
				 size_t *size, size_t want)
{
	mem_istream_t *mem = (mem_istream_t *)strm;
	size_t have = mem->size - mem->offset;

	if (have > mem->bufsz)
		have = mem->bufsz;

	if (want > have)
		want = have;

	if (mem->visible == 0 || mem->visible < want) {
		memcpy(mem->buffer + mem->visible,
		       (const char *)mem->data + mem->offset + mem->visible,
		       have - mem->visible);
		mem->visible = have;
	}

	*out = mem->buffer;
	*size = mem->visible;
	return (mem->visible == 0) ? 1 : 0;
}

static void mem_advance_buffer(sqfs_istream_t *strm, size_t count)
{
	mem_istream_t *mem = (mem_istream_t *)strm;

	assert(count <= mem->visible);

	if (count > 0 && count < mem->visible)
		memmove(mem->buffer, mem->buffer + count, mem->visible - count);

	mem->offset += count;
	mem->visible -= count;

	if (mem->visible < mem->bufsz) {
		memset(mem->buffer + mem->visible, 0,
		       mem->bufsz - mem->visible);
	}
}

static const char *mem_in_get_filename(sqfs_istream_t *strm)
{
	return ((mem_istream_t *)strm)->name;
}

static void mem_in_destroy(sqfs_object_t *obj)
{
	free(((mem_istream_t *)obj)->buffer);
	free(((mem_istream_t *)obj)->name);
	free(obj);
}

sqfs_istream_t *istream_memory_create(const char *name, size_t bufsz,
				      const void *data, size_t size)
{
	mem_istream_t *mem = calloc(1, sizeof(*mem));
	sqfs_istream_t *strm = (sqfs_istream_t *)mem;

	if (mem == NULL)
		return NULL;

	sqfs_object_init(mem, mem_in_destroy, NULL);

	mem->name = strdup(name);
	if (mem->name == NULL)
		return sqfs_drop(mem);

	mem->buffer = malloc(bufsz);
	if (mem->buffer == NULL)
		return sqfs_drop(mem);

	mem->data = data;
	mem->size = size;
	mem->bufsz = bufsz;
	strm->get_buffered_data = mem_get_buffered_data;
	strm->advance_buffer = mem_advance_buffer;
	strm->get_filename = mem_in_get_filename;
	return strm;
}

int istream_open_stdin(sqfs_istream_t **out)
{
	sqfs_file_handle_t hnd = GetStdHandle(STD_INPUT_HANDLE);

	return sqfs_istream_open_handle(out, "stdin", hnd, 0);
}

int ostream_open_stdout(sqfs_ostream_t **out)
{
	sqfs_file_handle_t hnd = GetStdHandle(STD_OUTPUT_HANDLE);

	return sqfs_ostream_open_handle(out, "stdout", hnd,
					SQFS_FILE_OPEN_NO_SPARSE);
}
