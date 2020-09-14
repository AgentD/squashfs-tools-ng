/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gzip.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#include <zlib.h>

typedef struct {
	ostream_comp_t base;

	z_stream strm;
} ostream_gzip_t;

static int flush_inbuf(ostream_comp_t *base, bool finish)
{
	ostream_gzip_t *gzip = (ostream_gzip_t *)base;
	size_t have;
	int ret;

	gzip->strm.avail_in = base->inbuf_used;
	gzip->strm.next_in = base->inbuf;

	do {
		gzip->strm.avail_out = BUFSZ;
		gzip->strm.next_out = base->outbuf;

		ret = deflate(&gzip->strm, finish ? Z_FINISH : Z_NO_FLUSH);

		if (ret == Z_STREAM_ERROR) {
			fprintf(stderr,
				"%s: internal error in gzip compressor.\n",
				base->wrapped->get_filename(base->wrapped));
			return -1;
		}

		have = BUFSZ - gzip->strm.avail_out;

		if (base->wrapped->append(base->wrapped, base->outbuf, have))
			return -1;
	} while (gzip->strm.avail_out == 0);

	base->inbuf_used = 0;
	return 0;
}

static void cleanup(ostream_comp_t *base)
{
	ostream_gzip_t *gzip = (ostream_gzip_t *)base;

	deflateEnd(&gzip->strm);
}

ostream_comp_t *ostream_gzip_create(const char *filename)
{
	ostream_gzip_t *gzip = calloc(1, sizeof(*gzip));
	ostream_comp_t *base = (ostream_comp_t *)gzip;
	int ret;

	if (gzip == NULL) {
		fprintf(stderr, "%s: creating gzip wrapper: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	ret = deflateInit2(&gzip->strm, 9, Z_DEFLATED, 16 + 15, 8,
			   Z_DEFAULT_STRATEGY);
	if (ret != Z_OK) {
		fprintf(stderr,
			"%s: internal error creating gzip compressor.\n",
			filename);
		free(gzip);
		return NULL;
	}

	base->flush_inbuf = flush_inbuf;
	base->cleanup = cleanup;
	return base;
}
