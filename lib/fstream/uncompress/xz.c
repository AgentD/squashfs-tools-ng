/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xz.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#include <lzma.h>

typedef struct {
	istream_comp_t base;

	lzma_stream strm;
} istream_xz_t;

static int precache(istream_t *base)
{
	istream_xz_t *xz = (istream_xz_t *)base;
	istream_t *wrapped = ((istream_comp_t *)base)->wrapped;
	lzma_action action;
	lzma_ret ret_xz;
	int ret;

	for (;;) {
		ret = istream_precache(wrapped);
		if (ret != 0)
			return ret;

		action = wrapped->eof ? LZMA_FINISH : LZMA_RUN;

		xz->strm.avail_in = wrapped->buffer_used;
		xz->strm.next_in = wrapped->buffer;

		xz->strm.avail_out = BUFSZ - base->buffer_used;
		xz->strm.next_out = base->buffer + base->buffer_used;

		ret_xz = lzma_code(&xz->strm, action);

		base->buffer_used = BUFSZ - xz->strm.avail_out;
		wrapped->buffer_offset = wrapped->buffer_used -
					 xz->strm.avail_in;

		if (ret_xz == LZMA_BUF_ERROR)
			break;

		if (ret_xz == LZMA_STREAM_END) {
			base->eof = true;
			break;
		}

		if (ret_xz != LZMA_OK) {
			fprintf(stderr,
				"%s: internal error in xz decoder.\n",
				wrapped->get_filename(wrapped));
			return -1;
		}
	}

	return 0;
}

static void cleanup(istream_comp_t *base)
{
	istream_xz_t *xz = (istream_xz_t *)base;

	lzma_end(&xz->strm);
}

istream_comp_t *istream_xz_create(const char *filename)
{
	istream_xz_t *xz = calloc(1, sizeof(*xz));
	istream_comp_t *base = (istream_comp_t *)xz;
	sqfs_u64 memlimit = 65 * 1024 * 1024;
	lzma_ret ret_xz;

	if (xz == NULL) {
		fprintf(stderr, "%s: creating xz decoder: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	ret_xz = lzma_stream_decoder(&xz->strm, memlimit, LZMA_CONCATENATED);

	if (ret_xz != LZMA_OK) {
		fprintf(stderr,
			"%s: error initializing xz decoder.\n",
			filename);
		free(xz);
		return NULL;
	}

	((istream_t *)base)->precache = precache;
	base->cleanup = cleanup;
	return base;
}
