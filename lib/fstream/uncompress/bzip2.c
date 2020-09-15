/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bzip2.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#include <bzlib.h>

typedef struct {
	istream_comp_t base;

	bz_stream strm;
} istream_bzip2_t;

static int precache(istream_t *base)
{
	istream_bzip2_t *bzip2 = (istream_bzip2_t *)base;
	istream_t *wrapped = ((istream_comp_t *)base)->wrapped;
	int ret;

	for (;;) {
		ret = istream_precache(wrapped);
		if (ret != 0)
			return ret;

		bzip2->strm.next_in = (char *)wrapped->buffer;
		bzip2->strm.avail_in = wrapped->buffer_used;

		bzip2->strm.next_out = (char *)base->buffer + base->buffer_used;
		bzip2->strm.avail_out = BUFSZ - base->buffer_used;

		if (bzip2->strm.avail_out < 1)
			break;

		ret = BZ2_bzDecompress(&bzip2->strm);

		if (ret < 0) {
			fprintf(stderr, "%s: internal error in bzip2 "
				"decompressor.\n",
				wrapped->get_filename(wrapped));
			return -1;
		}

		base->buffer_used = BUFSZ - bzip2->strm.avail_out;
		wrapped->buffer_offset = wrapped->buffer_used -
					 bzip2->strm.avail_in;

		if (ret == BZ_STREAM_END) {
			base->eof = true;
			break;
		}
	}

	return 0;
}

static void cleanup(istream_comp_t *base)
{
	istream_bzip2_t *bzip2 = (istream_bzip2_t *)base;

	BZ2_bzDecompressEnd(&bzip2->strm);
}

istream_comp_t *istream_bzip2_create(const char *filename)
{
	istream_bzip2_t *bzip2 = calloc(1, sizeof(*bzip2));
	istream_comp_t *base = (istream_comp_t *)bzip2;

	if (bzip2 == NULL) {
		fprintf(stderr, "%s: creating bzip2 compressor: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	if (BZ2_bzDecompressInit(&bzip2->strm, 0, 0) != BZ_OK) {
		fprintf(stderr, "%s: error initializing bzip2 decompressor.\n",
			filename);
		free(bzip2);
		return NULL;
	}

	((istream_t *)base)->precache = precache;
	base->cleanup = cleanup;
	return base;
}
