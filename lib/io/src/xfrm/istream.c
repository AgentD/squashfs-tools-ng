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

	sqfs_u8 uncompressed[BUFSZ];
} istream_xfrm_t;

static int xfrm_precache(istream_t *base)
{
	istream_xfrm_t *xfrm = (istream_xfrm_t *)base;
	int ret, sret;

	assert(base->buffer >= xfrm->uncompressed);
	assert(base->buffer <= (xfrm->uncompressed + BUFSZ));
	assert(base->buffer_used <= BUFSZ);
	assert((size_t)(base->buffer - xfrm->uncompressed) <=
	       (BUFSZ - base->buffer_used));

	if (base->buffer_used > 0)
		memmove(xfrm->uncompressed, base->buffer, base->buffer_used);

	base->buffer = xfrm->uncompressed;

	for (;;) {
		sqfs_u32 in_off = 0, out_off = base->buffer_used;
		int mode = XFRM_STREAM_FLUSH_NONE;
		const sqfs_u8 *ptr;
		size_t avail;

		ret = istream_get_buffered_data(xfrm->wrapped, &ptr, &avail,
						sizeof(xfrm->uncompressed));
		if (ret < 0)
			return ret;
		if (ret > 0) {
			mode = XFRM_STREAM_FLUSH_FULL;
			avail = 0;
		}

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

		base->buffer_used = out_off;

		sret = istream_advance_buffer(xfrm->wrapped, in_off);
		if (sret != 0)
			return sret;

		if (ret == XFRM_STREAM_BUFFER_FULL || out_off >= BUFSZ)
			break;

		if (mode == XFRM_STREAM_FLUSH_FULL)
			break;
	}

	return 0;
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

	base->precache = xfrm_precache;
	base->get_filename = xfrm_get_filename;
	base->buffer = stream->uncompressed;
	return base;
fail:
	fprintf(stderr, "%s: error initializing decompressor stream.\n",
		strm->get_filename(strm));
	return NULL;
}
