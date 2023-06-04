/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar/tar.h"
#include "sqfs/error.h"
#include "util/util.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

typedef struct {
	dir_iterator_t base;
	tar_header_decoded_t current;
	istream_t *stream;
	int state;

	/* File I/O wrapper related */
	bool locked;

	sqfs_u64 record_size;
	sqfs_u64 file_size;
	sqfs_u64 offset;

	size_t padding;
	size_t last_chunk;
	bool last_sparse;
} tar_iterator_t;

typedef struct {
	istream_t base;

	tar_iterator_t *parent;

	sqfs_u8 buffer[4096];
} tar_istream_t;

static bool is_sparse_region(const tar_iterator_t *tar, sqfs_u64 *count)
{
	const sparse_map_t *it;

	*count = tar->file_size - tar->offset;
	if (tar->current.sparse == NULL)
		return false;

	for (it = tar->current.sparse; it != NULL; it = it->next) {
		if (tar->offset >= it->offset) {
			sqfs_u64 diff = tar->offset - it->offset;

			if (diff < it->count) {
				*count = it->count - diff;
				return false;
			}
		}
	}

	for (it = tar->current.sparse; it != NULL; it = it->next) {
		if (tar->offset < it->offset) {
			sqfs_u64 diff = it->offset - tar->offset;

			if (diff < *count)
				*count = diff;
		}
	}

	return true;
}

static int data_available(tar_iterator_t *tar, sqfs_u64 want, sqfs_u64 *out)
{
	sqfs_u64 avail = tar->stream->buffer_used - tar->stream->buffer_offset;

	if ((want > avail) &&
	    ((tar->stream->buffer_offset > 0) || avail == 0)) {
		if (istream_precache(tar->stream)) {
			tar->state = SQFS_ERROR_IO;
			return -1;
		}

		if (tar->stream->buffer_used == 0 && tar->stream->eof) {
			tar->state = SQFS_ERROR_CORRUPTED;
			return -1;
		}

		avail = tar->stream->buffer_used;
	}

	*out = avail <= want ? avail : want;
	return 0;
}

/*****************************************************************************/

static const char *strm_get_filename(istream_t *strm)
{
	return ((tar_istream_t *)strm)->parent->current.name;
}

static int strm_precache(istream_t *strm)
{
	tar_istream_t *tar = (tar_istream_t *)strm;
	sqfs_u64 diff;

	strm->buffer_used -= strm->buffer_offset;
	strm->buffer_offset = 0;

	if (strm->eof)
		return 0;

	tar->parent->offset += tar->parent->last_chunk;

	if (!tar->parent->last_sparse) {
		tar->parent->stream->buffer_offset += tar->parent->last_chunk;
		tar->parent->record_size -= tar->parent->last_chunk;
	}

	if (tar->parent->offset >= tar->parent->file_size)
		goto out_eof;

	if (is_sparse_region(tar->parent, &diff)) {
		if (diff > sizeof(tar->buffer))
			diff = sizeof(tar->buffer);

		strm->buffer = tar->buffer;
		strm->buffer_used = diff;
		tar->parent->last_chunk = diff;
		tar->parent->last_sparse = true;

		memset(tar->buffer, 0, diff);
	} else {
		if (data_available(tar->parent, diff, &diff))
			goto out_eof;

		strm->buffer = tar->parent->stream->buffer +
			tar->parent->stream->buffer_offset;
		strm->buffer_used = diff;
		tar->parent->last_chunk = diff;
		tar->parent->last_sparse = false;
	}

	return 0;
out_eof:
	strm->eof = true;
	strm->buffer_used = 0;
	strm->buffer = tar->buffer;
	tar->parent->locked = false;
	return tar->parent->state < 0 ? -1 : 0;
}

static void strm_destroy(sqfs_object_t *obj)
{
	tar_istream_t *tar = (tar_istream_t *)obj;

	tar->parent->locked = false;
	sqfs_drop(tar->parent);
	free(tar);
}

/*****************************************************************************/

static int it_next(dir_iterator_t *it, dir_entry_t **out)
{
	tar_iterator_t *tar = (tar_iterator_t *)it;
	int ret;

	*out = NULL;
	if (tar->locked)
		return SQFS_ERROR_SEQUENCE;

	if (tar->state != 0)
		return tar->state;
retry:
	if (tar->record_size > 0) {
		ret = istream_skip(tar->stream, tar->record_size);
		if (ret)
			goto fail;
	}

	if (tar->padding > 0) {
		ret = istream_skip(tar->stream, tar->padding);
		if (ret)
			goto fail;
	}

	clear_header(&(tar->current));
	ret = read_header(tar->stream, &(tar->current));
	if (ret != 0)
		goto fail;

	tar->offset = 0;
	tar->last_chunk = 0;
	tar->last_sparse = false;
	tar->record_size = tar->current.record_size;
	tar->file_size = tar->current.actual_size;
	tar->padding = tar->current.record_size % 512;
	if (tar->padding > 0)
		tar->padding = 512 - tar->padding;

	if (tar->current.unknown_record)
		goto retry;

	if (canonicalize_name(tar->current.name) != 0) {
		tar->state = SQFS_ERROR_CORRUPTED;
		return tar->state;
	}

	*out = dir_entry_create(tar->current.name);
	if ((*out) == NULL) {
		tar->state = SQFS_ERROR_ALLOC;
		return tar->state;
	}

	(*out)->mtime = tar->current.mtime;
	(*out)->rdev = tar->current.devno;
	(*out)->uid = tar->current.uid;
	(*out)->gid = tar->current.gid;
	(*out)->mode = tar->current.mode;

	if (tar->current.is_hard_link) {
		(*out)->mode = (S_IFLNK | 0777);
		(*out)->flags |= DIR_ENTRY_FLAG_HARD_LINK;
	}

	if (S_ISREG((*out)->mode))
		(*out)->size = tar->current.actual_size;

	return 0;
fail:
	tar->state = ret < 0 ? SQFS_ERROR_IO : 1;
	return tar->state;
}

static int it_read_link(dir_iterator_t *it, char **out)
{
	tar_iterator_t *tar = (tar_iterator_t *)it;

	*out = NULL;
	if (tar->locked)
		return SQFS_ERROR_SEQUENCE;

	if (tar->state != 0 || tar->current.link_target == NULL)
		return tar->state < 0 ? tar->state : SQFS_ERROR_NO_ENTRY;

	*out = strdup(tar->current.link_target);
	return (*out == NULL) ? SQFS_ERROR_ALLOC : 0;
}

static int it_open_subdir(dir_iterator_t *it, dir_iterator_t **out)
{
	(void)it;
	*out = NULL;
	return SQFS_ERROR_UNSUPPORTED;
}

static void it_ignore_subdir(dir_iterator_t *it)
{
	(void)it;
	/* TODO: skip list */
}

static int it_open_file_ro(dir_iterator_t *it, istream_t **out)
{
	tar_iterator_t *tar = (tar_iterator_t *)it;
	tar_istream_t *strm;

	*out = NULL;
	if (tar->locked)
		return SQFS_ERROR_SEQUENCE;

	if (tar->state != 0)
		return tar->state < 0 ? tar->state : SQFS_ERROR_NO_ENTRY;

	if (!S_ISREG(tar->current.mode))
		return SQFS_ERROR_NOT_FILE;

	strm = calloc(1, sizeof(*strm));
	if (strm == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(strm, strm_destroy, NULL);
	strm->parent = sqfs_grab(tar);

	((istream_t *)strm)->precache = strm_precache;
	((istream_t *)strm)->get_filename = strm_get_filename;
	((istream_t *)strm)->buffer = strm->buffer;

	tar->locked = true;
	*out = (istream_t *)strm;
	return 0;
}

static int it_read_xattr(dir_iterator_t *it, dir_entry_xattr_t **out)
{
	tar_iterator_t *tar = (tar_iterator_t *)it;

	*out = NULL;
	if (tar->locked)
		return SQFS_ERROR_SEQUENCE;

	if (tar->state != 0)
		return tar->state < 0 ? tar->state : SQFS_ERROR_NO_ENTRY;

	if (tar->current.xattr != NULL) {
		*out = dir_entry_xattr_list_copy(tar->current.xattr);
		if (*out == NULL)
			return SQFS_ERROR_ALLOC;
	}

	return 0;
}

static void it_destroy(sqfs_object_t *obj)
{
	tar_iterator_t *tar = (tar_iterator_t *)obj;

	clear_header(&(tar->current));
	sqfs_drop(tar->stream);
	free(tar);
}

dir_iterator_t *tar_open_stream(istream_t *stream)
{
	tar_iterator_t *tar = calloc(1, sizeof(*tar));
	dir_iterator_t *it = (dir_iterator_t *)tar;

	if (tar == NULL)
		return NULL;

	sqfs_object_init(it, it_destroy, NULL);
	tar->stream = sqfs_grab(stream);
	it->next = it_next;
	it->read_link = it_read_link;
	it->open_subdir = it_open_subdir;
	it->ignore_subdir = it_ignore_subdir;
	it->open_file_ro = it_open_file_ro;
	it->read_xattr = it_read_xattr;

	return it;
}
