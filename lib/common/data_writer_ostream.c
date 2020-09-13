/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer_ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "common.h"

#include <stdlib.h>

typedef struct{
	ostream_t base;

	sqfs_block_processor_t *proc;
	const char *filename;
} data_writer_ostream_t;

static int stream_append(ostream_t *base, const void *data, size_t size)
{
	data_writer_ostream_t *strm = (data_writer_ostream_t *)base;
	int ret;

	ret = sqfs_block_processor_append(strm->proc, data, size);

	if (ret != 0) {
		sqfs_perror(strm->filename, NULL, ret);
		return -1;
	}

	return 0;
}

static int stream_flush(ostream_t *base)
{
	data_writer_ostream_t *strm = (data_writer_ostream_t *)base;
	int ret;

	ret = sqfs_block_processor_end_file(strm->proc);

	if (ret != 0) {
		sqfs_perror(strm->filename, NULL, ret);
		return -1;
	}

	return 0;
}

static const char *stream_get_filename(ostream_t *base)
{
	data_writer_ostream_t *strm = (data_writer_ostream_t *)base;

	return strm->filename;
}

static void stream_destroy(sqfs_object_t *base)
{
	free(base);
}

ostream_t *data_writer_ostream_create(const char *filename,
				      sqfs_block_processor_t *proc,
				      sqfs_inode_generic_t **inode,
				      int flags)
{
	data_writer_ostream_t *strm = calloc(1, sizeof(*strm));
	sqfs_object_t *obj = (sqfs_object_t *)strm;
	ostream_t *base = (ostream_t *)strm;
	int ret;

	if (strm == NULL) {
		perror(filename);
		return NULL;
	}

	ret = sqfs_block_processor_begin_file(proc, inode, NULL, flags);

	if (ret != 0) {
		sqfs_perror(filename, NULL, ret);
		free(strm);
		return NULL;
	}

	strm->proc = proc;
	strm->filename = filename;
	base->append = stream_append;
	base->flush = stream_flush;
	base->get_filename = stream_get_filename;
	obj->destroy = stream_destroy;
	return base;
}
