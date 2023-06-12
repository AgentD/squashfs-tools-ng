/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"
#include "sqfs/io.h"

typedef struct ostream_xfrm_t {
	sqfs_ostream_t base;

	sqfs_ostream_t *wrapped;
	xfrm_stream_t *xfrm;

	size_t inbuf_used;

	sqfs_u8 inbuf[BUFSZ];
	sqfs_u8 outbuf[BUFSZ];
} ostream_xfrm_t;

static int flush_inbuf(ostream_xfrm_t *xfrm, bool finish)
{
	const sqfs_u32 avail_out = sizeof(xfrm->outbuf);
	const sqfs_u32 avail_in = xfrm->inbuf_used;
	const int mode = finish ? XFRM_STREAM_FLUSH_FULL :
		XFRM_STREAM_FLUSH_NONE;
	sqfs_u32 off_in = 0, off_out = 0;
	int ret;

	while (finish || off_in < avail_in) {
		ret = xfrm->xfrm->process_data(xfrm->xfrm,
					       xfrm->inbuf + off_in,
					       avail_in - off_in,
					       xfrm->outbuf + off_out,
					       avail_out - off_out,
					       &off_in, &off_out, mode);

		if (ret == XFRM_STREAM_ERROR) {
			fprintf(stderr,
				"%s: internal error in compressor.\n",
				xfrm->wrapped->get_filename(xfrm->wrapped));
			return -1;
		}

		if (xfrm->wrapped->append(xfrm->wrapped, xfrm->outbuf, off_out))
			return -1;

		off_out = 0;

		if (ret == XFRM_STREAM_END)
			break;
	}

	if (off_out > 0) {
		if (xfrm->wrapped->append(xfrm->wrapped, xfrm->outbuf, off_out))
			return -1;
	}

	if (off_in < avail_in) {
		memmove(xfrm->inbuf, xfrm->inbuf + off_in, avail_in - off_in);
		xfrm->inbuf_used -= off_in;
	} else {
		xfrm->inbuf_used = 0;
	}

	return 0;
}

static int xfrm_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	ostream_xfrm_t *xfrm = (ostream_xfrm_t *)strm;
	size_t diff;

	while (size > 0) {
		if (xfrm->inbuf_used >= BUFSZ) {
			if (flush_inbuf(xfrm, false))
				return -1;
		}

		diff = BUFSZ - xfrm->inbuf_used;

		if (diff > size)
			diff = size;

		if (data == NULL) {
			memset(xfrm->inbuf + xfrm->inbuf_used, 0, diff);
		} else {
			memcpy(xfrm->inbuf + xfrm->inbuf_used, data, diff);
			data = (const char *)data + diff;
		}

		xfrm->inbuf_used += diff;
		size -= diff;
	}

	return 0;
}

static int xfrm_flush(sqfs_ostream_t *strm)
{
	ostream_xfrm_t *xfrm = (ostream_xfrm_t *)strm;

	if (xfrm->inbuf_used > 0) {
		if (flush_inbuf(xfrm, true))
			return -1;
	}

	return xfrm->wrapped->flush(xfrm->wrapped);
}

static const char *xfrm_get_filename(sqfs_ostream_t *strm)
{
	ostream_xfrm_t *xfrm = (ostream_xfrm_t *)strm;

	return xfrm->wrapped->get_filename(xfrm->wrapped);
}

static void xfrm_destroy(sqfs_object_t *obj)
{
	ostream_xfrm_t *xfrm = (ostream_xfrm_t *)obj;

	sqfs_drop(xfrm->wrapped);
	sqfs_drop(xfrm->xfrm);
	free(xfrm);
}

sqfs_ostream_t *ostream_xfrm_create(sqfs_ostream_t *strm, xfrm_stream_t *xfrm)
{
	ostream_xfrm_t *stream = calloc(1, sizeof(*stream));
	sqfs_ostream_t *base = (sqfs_ostream_t *)stream;

	if (stream == NULL)
		goto fail;

	sqfs_object_init(stream, xfrm_destroy, NULL);

	stream->wrapped = sqfs_grab(strm);
	stream->xfrm = sqfs_grab(xfrm);
	stream->inbuf_used = 0;
	base->append = xfrm_append;
	base->flush = xfrm_flush;
	base->get_filename = xfrm_get_filename;
	return base;
fail:
	fprintf(stderr, "%s: error initializing compressor.\n",
		strm->get_filename(strm));
	return NULL;
}
