/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gzip.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#include <zlib.h>

typedef struct {
	istream_comp_t base;

	z_stream strm;
} istream_gzip_t;

static int precache(istream_t *base)
{
	istream_t *wrapped = ((istream_comp_t *)base)->wrapped;
	istream_gzip_t *gzip = (istream_gzip_t *)base;
	int ret;

	for (;;) {
		ret = istream_precache(wrapped);
		if (ret != 0)
			return ret;

		gzip->strm.avail_in = wrapped->buffer_used;
		gzip->strm.next_in = wrapped->buffer;

		gzip->strm.avail_out = BUFSZ - base->buffer_used;
		gzip->strm.next_out = base->buffer + base->buffer_used;

		ret = inflate(&gzip->strm, Z_NO_FLUSH);

		wrapped->buffer_offset = wrapped->buffer_used -
					 gzip->strm.avail_in;

		base->buffer_used = BUFSZ - gzip->strm.avail_out;

		if (ret == Z_BUF_ERROR)
			break;

		if (ret == Z_STREAM_END) {
			base->eof = true;
			break;
		}

		if (ret != Z_OK) {
			fprintf(stderr,
				"%s: internal error in gzip decoder.\n",
				wrapped->get_filename(wrapped));
			return -1;
		}
	}

	return 0;
}

static void cleanup(istream_comp_t *base)
{
	istream_gzip_t *gzip = (istream_gzip_t *)base;

	inflateEnd(&gzip->strm);
}

istream_comp_t *istream_gzip_create(const char *filename)
{
	istream_gzip_t *gzip = calloc(1, sizeof(*gzip));
	istream_comp_t *base = (istream_comp_t *)gzip;
	int ret;

	if (gzip == NULL) {
		fprintf(stderr, "%s: creating gzip decoder: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	ret = inflateInit2(&gzip->strm, 16 + 15);
	if (ret != Z_OK) {
		fprintf(stderr,
			"%s: internal error creating gzip reader.\n",
			filename);
		free(gzip);
		return NULL;
	}

	((istream_t *)base)->precache = precache;
	base->cleanup = cleanup;
	return base;
}
