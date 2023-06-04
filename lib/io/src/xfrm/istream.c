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
	int ret;

	if (base->buffer_offset >= base->buffer_used) {
		base->buffer_offset = 0;
		base->buffer_used = 0;
	} else if (base->buffer_offset > 0) {
		memmove(base->buffer,
			base->buffer + base->buffer_offset,
			base->buffer_used - base->buffer_offset);

		base->buffer_used -= base->buffer_offset;
		base->buffer_offset = 0;
	}

	if (base->eof)
		return 0;

	ret = istream_precache(xfrm->wrapped);
	if (ret != 0)
		return ret;

	for (;;) {
		const sqfs_u32 in_sz = xfrm->wrapped->buffer_used;
		const sqfs_u32 out_sz = sizeof(xfrm->uncompressed);
		sqfs_u32 in_off = 0, out_off = base->buffer_used;
		int mode = XFRM_STREAM_FLUSH_NONE;

		if (xfrm->wrapped->eof)
			mode = XFRM_STREAM_FLUSH_FULL;

		ret = xfrm->xfrm->process_data(xfrm->xfrm,
					       xfrm->wrapped->buffer, in_sz,
					       base->buffer + out_off,
					       out_sz - out_off,
					       &in_off, &out_off, mode);

		if (ret == XFRM_STREAM_ERROR) {
			fprintf(stderr, "%s: internal error in decompressor.\n",
				base->get_filename(base));
			return -1;
		}

		base->buffer_used = out_off;
		xfrm->wrapped->buffer_offset = in_off;

		if (ret == XFRM_STREAM_BUFFER_FULL || out_off >= out_sz)
			break;

		ret = istream_precache(xfrm->wrapped);
		if (ret != 0)
			return ret;

		if (xfrm->wrapped->eof && xfrm->wrapped->buffer_used == 0) {
			if (base->buffer_used == 0)
				base->eof = true;
			break;
		}
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
	base->eof = false;
	return base;
fail:
	fprintf(stderr, "%s: error initializing decompressor stream.\n",
		strm->get_filename(strm));
	return NULL;
}
