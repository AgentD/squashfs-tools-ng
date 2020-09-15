/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * bzip2.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

#include <bzlib.h>

typedef struct {
	ostream_comp_t base;

	bz_stream strm;
} ostream_bzip2_t;

static int flush_inbuf(ostream_comp_t *base, bool finish)
{
	ostream_bzip2_t *bzip2 = (ostream_bzip2_t *)base;
	size_t have;
	int ret;

	bzip2->strm.next_in = (char *)base->inbuf;
	bzip2->strm.avail_in = base->inbuf_used;

	for (;;) {
		bzip2->strm.next_out = (char *)base->outbuf;
		bzip2->strm.avail_out = sizeof(base->outbuf);

		ret = BZ2_bzCompress(&bzip2->strm, finish ? BZ_FINISH : BZ_RUN);

		if (ret < 0 && ret != BZ_OUTBUFF_FULL) {
			fprintf(stderr, "%s: internal error in bzip2 "
				"compressor.\n",
				base->wrapped->get_filename(base->wrapped));
			return -1;
		}

		have = sizeof(base->outbuf) - bzip2->strm.avail_out;

		if (base->wrapped->append(base->wrapped, base->outbuf, have))
			return -1;

		if (ret == BZ_STREAM_END || ret == BZ_OUTBUFF_FULL ||
		    bzip2->strm.avail_in == 0) {
			break;
		}
	}

	if (bzip2->strm.avail_in > 0) {
		memmove(base->inbuf, bzip2->strm.next_in,
			bzip2->strm.avail_in);
	}

	base->inbuf_used = bzip2->strm.avail_in;
	return 0;
}

static void cleanup(ostream_comp_t *base)
{
	ostream_bzip2_t *bzip2 = (ostream_bzip2_t *)base;

	BZ2_bzCompressEnd(&bzip2->strm);
}

ostream_comp_t *ostream_bzip2_create(const char *filename)
{
	ostream_bzip2_t *bzip2 = calloc(1, sizeof(*bzip2));
	ostream_comp_t *base = (ostream_comp_t *)bzip2;

	if (bzip2 == NULL) {
		fprintf(stderr, "%s: creating bzip2 compressor: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	if (BZ2_bzCompressInit(&bzip2->strm, 9, 0, 30) != BZ_OK) {
		fprintf(stderr, "%s: error initializing bzip2 compressor.\n",
			filename);
		free(bzip2);
		return NULL;
	}

	base->flush_inbuf = flush_inbuf;
	base->cleanup = cleanup;
	return base;
}
