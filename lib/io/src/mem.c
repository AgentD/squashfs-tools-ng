/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mem.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "io/mem.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	istream_t base;

	sqfs_u8 *buffer;
	size_t bufsz;

	const void *data;
	size_t size;

	char *name;
} mem_istream_t;

static int mem_in_precache(istream_t *strm)
{
	mem_istream_t *mem = (mem_istream_t *)strm;
	size_t diff;

	if (strm->buffer_offset >= strm->buffer_used) {
		strm->buffer_offset = 0;
		strm->buffer_used = 0;
	} else if (strm->buffer_offset > 0) {
		memmove(strm->buffer,
			strm->buffer + strm->buffer_offset,
			strm->buffer_used - strm->buffer_offset);

		strm->buffer_used -= strm->buffer_offset;
		strm->buffer_offset = 0;
	}

	diff = mem->bufsz - strm->buffer_used;
	if (diff > mem->size)
		diff = mem->size;

	if (diff > 0) {
		memcpy(mem->buffer + strm->buffer_used, mem->data, diff);
		strm->buffer_used += diff;
		mem->data = (const char *)mem->data + diff;
		mem->size -= diff;
	}

	if (mem->size == 0)
		strm->eof = true;

	return 0;
}

static const char *mem_in_get_filename(istream_t *strm)
{
	return ((mem_istream_t *)strm)->name;
}

static void mem_in_destroy(sqfs_object_t *obj)
{
	free(((mem_istream_t *)obj)->buffer);
	free(((mem_istream_t *)obj)->name);
	free(obj);
}

istream_t *istream_memory_create(const char *name, size_t bufsz,
				 const void *data, size_t size)
{
	mem_istream_t *mem = calloc(1, sizeof(*mem));
	istream_t *strm = (istream_t *)mem;

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
	strm->buffer = mem->buffer;
	strm->precache = mem_in_precache;
	strm->get_filename = mem_in_get_filename;
	return strm;
}
