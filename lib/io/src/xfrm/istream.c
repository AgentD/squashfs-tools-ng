/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

typedef struct istream_xfrm_t {
	istream_t base;

	istream_t *wrapped;
	xfrm_stream_t *xfrm;

	size_t buffer_offset;
	size_t buffer_used;
	sqfs_u8 uncompressed[BUFSZ];
} istream_xfrm_t;

static int precache(istream_t *base)
{
	istream_xfrm_t *xfrm = (istream_xfrm_t *)base;
	int ret;

	if (xfrm->buffer_offset > 0 &&
	    xfrm->buffer_offset < xfrm->buffer_used) {
		memmove(xfrm->uncompressed,
			xfrm->uncompressed + xfrm->buffer_offset,
			xfrm->buffer_used - xfrm->buffer_offset);
	}

	xfrm->buffer_used -= xfrm->buffer_offset;
	xfrm->buffer_offset = 0;

	for (;;) {
		sqfs_u32 in_off = 0, out_off = xfrm->buffer_used;
		int mode = XFRM_STREAM_FLUSH_NONE;
		const sqfs_u8 *ptr;
		size_t avail;

		ret = istream_get_buffered_data(xfrm->wrapped, &ptr, &avail,
						sizeof(xfrm->uncompressed));
		if (ret < 0)
			return ret;
		if (ret > 0)
			mode = XFRM_STREAM_FLUSH_FULL;

		ret = xfrm->xfrm->process_data(xfrm->xfrm,
					       ptr, avail,
					       xfrm->uncompressed + out_off,
					       BUFSZ - out_off,
					       &in_off, &out_off, mode);

		if (ret == XFRM_STREAM_ERROR) {
			fprintf(stderr, "%s: internal error in decompressor.\n",
				base->get_filename(base));
			return -1;
		}

		xfrm->buffer_used = out_off;
		istream_advance_buffer(xfrm->wrapped, in_off);

		if (ret == XFRM_STREAM_BUFFER_FULL || out_off >= BUFSZ)
			break;

		if (mode == XFRM_STREAM_FLUSH_FULL)
			break;
	}

	return 0;
}

static int xfrm_get_buffered_data(istream_t *strm, const sqfs_u8 **out,
				  size_t *size, size_t want)
{
	istream_xfrm_t *xfrm = (istream_xfrm_t *)strm;

	if (want > BUFSZ)
		want = BUFSZ;

	if (xfrm->buffer_used == 0 ||
	    (xfrm->buffer_used - xfrm->buffer_offset) < want) {
		int ret = precache(strm);
		if (ret)
			return ret;
	}

	*out = xfrm->uncompressed + xfrm->buffer_offset;
	*size = xfrm->buffer_used - xfrm->buffer_offset;
	return (*size == 0) ? 1 : 0;
}

static void xfrm_advance_buffer(istream_t *strm, size_t count)
{
	istream_xfrm_t *xfrm = (istream_xfrm_t *)strm;

	assert(count <= xfrm->buffer_used);

	xfrm->buffer_offset += count;

	assert(xfrm->buffer_offset <= xfrm->buffer_used);
}

static const char *xfrm_get_filename(istream_t *strm)
{
	istream_xfrm_t *xfrm = (istream_xfrm_t *)strm;

	return xfrm->wrapped->get_filename(xfrm->wrapped);
}

static void xfrm_destroy(sqfs_object_t *obj)
{
	istream_xfrm_t *xfrm = (istream_xfrm_t *)obj;

	sqfs_drop(xfrm->xfrm);
	sqfs_drop(xfrm->wrapped);
	free(xfrm);
}

istream_t *istream_xfrm_create(istream_t *strm, xfrm_stream_t *xfrm)
{
	istream_xfrm_t *stream = calloc(1, sizeof(*stream));
	istream_t *base = (istream_t *)stream;

	if (stream == NULL)
		goto fail;

	sqfs_object_init(stream, xfrm_destroy, NULL);

	stream->wrapped = sqfs_grab(strm);
	stream->xfrm = sqfs_grab(xfrm);

	base->get_buffered_data = xfrm_get_buffered_data;
	base->advance_buffer = xfrm_advance_buffer;
	base->get_filename = xfrm_get_filename;
	return base;
fail:
	fprintf(stderr, "%s: error initializing decompressor stream.\n",
		strm->get_filename(strm));
	return NULL;
}
