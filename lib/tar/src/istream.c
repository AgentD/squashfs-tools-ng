/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

#include <string.h>
#include <stdlib.h>

typedef struct {
	sqfs_u64 offset;
	sqfs_u64 count;
} sparse_ent_t;

typedef struct {
	istream_t base;

	istream_t *parent;

	char *filename;

	sparse_ent_t *sparse;
	size_t num_sparse;

	sqfs_u64 record_size;
	sqfs_u64 file_size;
	sqfs_u64 offset;

	size_t last_chunk;
	bool last_sparse;

	sqfs_u8 buffer[4096];
} tar_istream_t;

static bool is_sparse_region(tar_istream_t *tar, sqfs_u64 *count)
{
	size_t i;

	*count = tar->file_size - tar->offset;
	if (tar->num_sparse == 0)
		return false;

	for (i = 0; i < tar->num_sparse; ++i) {
		if (tar->offset >= tar->sparse[i].offset) {
			sqfs_u64 diff = tar->offset - tar->sparse[i].offset;

			if (diff < tar->sparse[i].count) {
				*count = tar->sparse[i].count - diff;
				return false;
			}
		}
	}

	for (i = 0; i < tar->num_sparse; ++i) {
		if (tar->offset < tar->sparse[i].offset) {
			sqfs_u64 diff = tar->sparse[i].offset - tar->offset;

			if (diff < *count)
				*count = diff;
		}
	}

	return true;
}

static int precache(istream_t *strm)
{
	tar_istream_t *tar = (tar_istream_t *)strm;
	sqfs_u64 diff, avail;

	tar->offset += tar->last_chunk;

	if (!tar->last_sparse) {
		tar->parent->buffer_offset += tar->last_chunk;
		tar->record_size -= tar->last_chunk;
	}

	if (tar->offset >= tar->file_size) {
		strm->eof = true;
		strm->buffer_used = 0;
		strm->buffer = tar->buffer;
		if (tar->record_size > 0)
			goto fail_rec_sz;
		return 0;
	}

	if (is_sparse_region(tar, &diff)) {
		if (diff > sizeof(tar->buffer))
			diff = sizeof(tar->buffer);

		strm->buffer = tar->buffer;
		strm->buffer_used = diff;
		tar->last_chunk = diff;
		tar->last_sparse = true;

		memset(tar->buffer, 0, diff);
	} else {
		if (diff > tar->record_size)
			goto fail_rec_sz;

		avail = tar->parent->buffer_used - tar->parent->buffer_offset;

		if ((diff > avail) &&
		    ((tar->parent->buffer_offset > 0) || avail == 0)) {
			if (istream_precache(tar->parent))
				return -1;

			if (tar->parent->buffer_used == 0 && tar->parent->eof)
				goto fail_eof;

			avail = tar->parent->buffer_used;
		}

		if (diff > avail)
			diff = avail;

		strm->buffer = tar->parent->buffer + tar->parent->buffer_offset;
		strm->buffer_used = diff;
		tar->last_chunk = diff;
		tar->last_sparse = false;
	}

	return 0;
fail_rec_sz:
	fprintf(stderr,
		"%s: missmatch in tar record size vs file size for `%s`.\n",
		istream_get_filename(tar->parent), istream_get_filename(strm));
	return -1;
fail_eof:
	fprintf(stderr, "%s: unexpected end-of-file while reading `%s`\n",
		istream_get_filename(tar->parent), istream_get_filename(strm));
	return -1;
}

static const char *get_filename(istream_t *strm)
{
	return ((tar_istream_t *)strm)->filename;
}

static void tar_istream_destroy(sqfs_object_t *obj)
{
	tar_istream_t *strm = (tar_istream_t *)obj;

	sqfs_drop(strm->parent);
	free(strm->sparse);
	free(strm->filename);
	free(strm);
}

istream_t *tar_record_istream_create(istream_t *parent,
				     const tar_header_decoded_t *hdr)
{
	tar_istream_t *strm;
	sparse_map_t *it;
	sqfs_u64 diff;
	size_t idx;

	strm = calloc(1, sizeof(*strm));
	if (strm == NULL)
		goto fail_oom;

	sqfs_object_init(strm, tar_istream_destroy, NULL);

	strm->filename = strdup(hdr->name);
	if (strm->filename == NULL)
		goto fail_oom;

	strm->num_sparse = 0;
	for (it = hdr->sparse; it != NULL; it = it->next)
		strm->num_sparse += 1;

	if (strm->num_sparse > 0) {
		strm->sparse = alloc_array(sizeof(strm->sparse[0]),
					   strm->num_sparse);
		if (strm->sparse == NULL)
			goto fail_oom;

		idx = 0;
		it = hdr->sparse;
		while (it != NULL && idx < strm->num_sparse) {
			strm->sparse[idx].offset = it->offset;
			strm->sparse[idx].count = it->count;
			++idx;
			it = it->next;
		}
	}

	for (idx = 1; idx < strm->num_sparse; ++idx) {
		if (strm->sparse[idx].offset <= strm->sparse[idx - 1].offset)
			goto fail_sparse;

		diff = strm->sparse[idx].offset - strm->sparse[idx - 1].offset;

		if (diff < strm->sparse[idx - 1].count)
			goto fail_sparse;
	}

	strm->record_size = hdr->record_size;
	strm->file_size = hdr->actual_size;
	strm->parent = sqfs_grab(parent);

	((istream_t *)strm)->precache = precache;
	((istream_t *)strm)->get_filename = get_filename;
	((istream_t *)strm)->buffer = strm->buffer;
	((istream_t *)strm)->eof = false;
	return (istream_t *)strm;
fail_sparse:
	fprintf(stderr, "%s: sparse map is not ordered or overlapping!\n",
		hdr->name);
	goto fail;
fail_oom:
	fputs("tar istream create: out-of-memory\n", stderr);
	goto fail;
fail:
	if (strm != NULL) {
		free(strm->filename);
		free(strm);
	}
	return NULL;
}
