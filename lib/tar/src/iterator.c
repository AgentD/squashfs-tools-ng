/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xfrm/compress.h"
#include "tar/format.h"
#include "tar/tar.h"
#include "sqfs/error.h"
#include "sqfs/xattr.h"
#include "util/util.h"
#include "io/xfrm.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>

typedef struct {
	dir_iterator_t base;
	tar_header_decoded_t current;
	sqfs_istream_t *stream;
	int state;

	/* File I/O wrapper related */
	bool locked;

	sqfs_u64 record_size;
	sqfs_u64 file_size;
	sqfs_u64 offset;

	size_t padding;
	bool last_sparse;
} tar_iterator_t;

typedef struct {
	sqfs_istream_t base;

	tar_iterator_t *parent;
	int state;

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

/*****************************************************************************/

static void drop_parent(tar_istream_t *tar, int state)
{
	if (tar->parent != NULL) {
		tar->parent->locked = false;
		tar->parent = sqfs_drop(tar->parent);

		if (state != 0 && tar->parent->state == 0)
			tar->parent->state = state;
	}

	tar->state = state;
}

static const char *strm_get_filename(sqfs_istream_t *strm)
{
	return ((tar_istream_t *)strm)->parent->current.name;
}

static int strm_get_buffered_data(sqfs_istream_t *strm, const sqfs_u8 **out,
				  size_t *size, size_t want)
{
	tar_istream_t *tar = (tar_istream_t *)strm;
	sqfs_u64 diff;
	int ret;

	if (tar->parent == NULL)
		return tar->state;

	if (tar->parent->offset >= tar->parent->file_size)
		goto out_eof;

	tar->parent->last_sparse = is_sparse_region(tar->parent, &diff);
	if (diff == 0)
		goto out_eof;
	if (diff > want)
		diff = want;

	if (tar->parent->last_sparse) {
		*out = tar->buffer;
		*size = (diff <= sizeof(tar->buffer)) ?
			diff : sizeof(tar->buffer);
	} else {
		ret = tar->parent->stream->
			get_buffered_data(tar->parent->stream,
					  out, size, diff);
		if (ret > 0)
			goto fail_borked;
		if (ret < 0)
			goto fail_io;
		if (*size > diff)
			*size = diff;
	}

	return 0;
fail_io:
	drop_parent(tar, ret);
	return tar->state;
fail_borked:
	drop_parent(tar, SQFS_ERROR_CORRUPTED);
	return tar->state;
out_eof:
	drop_parent(tar, 0);
	tar->state = 1;
	return 1;
}

static void strm_advance_buffer(sqfs_istream_t *strm, size_t count)
{
	tar_istream_t *tar = (tar_istream_t *)strm;

	if (!tar->parent->last_sparse) {
		tar->parent->stream->advance_buffer(tar->parent->stream, count);
		tar->parent->record_size -= count;
	}

	tar->parent->offset += count;
}

static void strm_destroy(sqfs_object_t *obj)
{
	tar_istream_t *tar = (tar_istream_t *)obj;

	drop_parent(tar, 0);
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

static int it_open_file_ro(dir_iterator_t *it, sqfs_istream_t **out)
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

	((sqfs_istream_t *)strm)->get_buffered_data = strm_get_buffered_data;
	((sqfs_istream_t *)strm)->advance_buffer = strm_advance_buffer;
	((sqfs_istream_t *)strm)->get_filename = strm_get_filename;

	tar->locked = true;
	*out = (sqfs_istream_t *)strm;
	return 0;
}

static int it_read_xattr(dir_iterator_t *it, sqfs_xattr_t **out)
{
	tar_iterator_t *tar = (tar_iterator_t *)it;

	*out = NULL;
	if (tar->locked)
		return SQFS_ERROR_SEQUENCE;

	if (tar->state != 0)
		return tar->state < 0 ? tar->state : SQFS_ERROR_NO_ENTRY;

	if (tar->current.xattr != NULL) {
		*out = sqfs_xattr_list_copy(tar->current.xattr);
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

/*****************************************************************************/

static int tar_probe(const sqfs_u8 *data, size_t size)
{
	size_t i, offset;

	if (size >= TAR_RECORD_SIZE) {
		for (i = 0; i < TAR_RECORD_SIZE; ++i) {
			if (data[i] != 0x00)
				break;
		}

		if (i == TAR_RECORD_SIZE) {
			data += TAR_RECORD_SIZE;
			size -= TAR_RECORD_SIZE;
		}
	}

	offset = offsetof(tar_header_t, magic);

	if (offset + 5 <= size) {
		if (memcmp(data + offset, "ustar", 5) == 0)
			return 1;
	}

	return 0;
}

dir_iterator_t *tar_open_stream(sqfs_istream_t *strm)
{
	tar_iterator_t *tar = calloc(1, sizeof(*tar));
	dir_iterator_t *it = (dir_iterator_t *)tar;
	xfrm_stream_t *xfrm = NULL;
	const sqfs_u8 *ptr;
	size_t size;
	int ret;

	if (tar == NULL)
		return NULL;

	sqfs_object_init(it, it_destroy, NULL);
	it->next = it_next;
	it->read_link = it_read_link;
	it->open_subdir = it_open_subdir;
	it->ignore_subdir = it_ignore_subdir;
	it->open_file_ro = it_open_file_ro;
	it->read_xattr = it_read_xattr;

	/* proble if the stream is compressed */
	ret = strm->get_buffered_data(strm, &ptr, &size,
				      sizeof(tar_header_t));
	if (ret != 0)
		goto out_strm;

	ret = tar_probe(ptr, size);
	if (ret > 0)
		goto out_strm;

	ret = xfrm_compressor_id_from_magic(ptr, size);
	if (ret <= 0)
		goto out_strm;

	/* auto-wrap a compressed source stream */
	xfrm = decompressor_stream_create(ret);
	if (xfrm == NULL) {
		sqfs_drop(it);
		return NULL;
	}

	tar->stream = istream_xfrm_create(strm, xfrm);
	if (tar->stream == NULL) {
		sqfs_drop(xfrm);
		sqfs_drop(it);
		return NULL;
	}

	return it;
out_strm:
	tar->stream = sqfs_grab(strm);
	return it;
}
