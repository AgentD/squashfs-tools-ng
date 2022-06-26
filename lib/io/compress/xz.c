/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xz.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#include <lzma.h>

typedef struct {
	ostream_comp_t base;

	lzma_stream strm;
} ostream_xz_t;

static int flush_inbuf(ostream_comp_t *base, bool finish)
{
	ostream_xz_t *xz = (ostream_xz_t *)base;
	lzma_ret ret_xz;
	size_t have;

	xz->strm.next_in = base->inbuf;
	xz->strm.avail_in = base->inbuf_used;

	do {
		xz->strm.next_out = base->outbuf;
		xz->strm.avail_out = BUFSZ;

		ret_xz = lzma_code(&xz->strm, finish ? LZMA_FINISH : LZMA_RUN);

		if ((ret_xz != LZMA_OK) && (ret_xz != LZMA_STREAM_END)) {
			fprintf(stderr,
				"%s: internal error in XZ compressor.\n",
				base->wrapped->get_filename(base->wrapped));
			return -1;
		}

		have = BUFSZ - xz->strm.avail_out;

		if (base->wrapped->append(base->wrapped, base->outbuf, have))
			return -1;
	} while (xz->strm.avail_out == 0);

	base->inbuf_used = 0;
	return 0;
}

static void cleanup(ostream_comp_t *base)
{
	ostream_xz_t *xz = (ostream_xz_t *)base;

	lzma_end(&xz->strm);
}

ostream_comp_t *ostream_xz_create(const char *filename)
{
	ostream_xz_t *xz = calloc(1, sizeof(*xz));
	ostream_comp_t *base = (ostream_comp_t *)xz;
	lzma_ret ret_xz;

	if (xz == NULL) {
		fprintf(stderr, "%s: creating xz wrapper: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	ret_xz = lzma_easy_encoder(&xz->strm, LZMA_PRESET_DEFAULT,
				   LZMA_CHECK_CRC64);
	if (ret_xz != LZMA_OK) {
		fprintf(stderr, "%s: error initializing XZ compressor\n",
			filename);
		free(xz);
		return NULL;
	}

	base->flush_inbuf = flush_inbuf;
	base->cleanup = cleanup;
	return base;
}
