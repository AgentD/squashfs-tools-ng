/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * zstd.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#include <zstd.h>

#ifdef HAVE_ZSTD_STREAM
typedef struct {
	istream_comp_t base;

	ZSTD_DStream* strm;
} istream_zstd_t;

static int precache(istream_t *base)
{
	istream_zstd_t *zstd = (istream_zstd_t *)base;
	istream_t *wrapped = ((istream_comp_t *)base)->wrapped;
	ZSTD_outBuffer out;
	ZSTD_inBuffer in;
	size_t ret;

	if (istream_precache(wrapped))
		return -1;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.src = wrapped->buffer;
	in.size = wrapped->buffer_used;

	out.dst = ((istream_comp_t *)base)->uncompressed + base->buffer_used;
	out.size = BUFSZ - base->buffer_used;

	ret = ZSTD_decompressStream(zstd->strm, &out, &in);

	if (ZSTD_isError(ret)) {
		fprintf(stderr, "%s: error in zstd decoder.\n",
			wrapped->get_filename(wrapped));
		return -1;
	}

	wrapped->buffer_offset = in.pos;
	base->buffer_used += out.pos;
	return 0;
}

static void cleanup(istream_comp_t *base)
{
	istream_zstd_t *zstd = (istream_zstd_t *)base;

	ZSTD_freeDStream(zstd->strm);
}

istream_comp_t *istream_zstd_create(const char *filename)
{
	istream_zstd_t *zstd = calloc(1, sizeof(*zstd));
	istream_comp_t *base = (istream_comp_t *)zstd;

	if (zstd == NULL) {
		fprintf(stderr, "%s: creating zstd decoder: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	zstd->strm = ZSTD_createDStream();
	if (zstd->strm == NULL) {
		fprintf(stderr, "%s: error creating zstd decoder.\n",
			filename);
		free(zstd);
		return NULL;
	}

	((istream_t *)base)->precache = precache;
	base->cleanup = cleanup;
	return base;
}
#endif /* HAVE_ZSTD_STREAM */
