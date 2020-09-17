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
	ostream_comp_t base;

	ZSTD_CStream *strm;
} ostream_zstd_t;

static int flush_inbuf(ostream_comp_t *base, bool finish)
{
	ostream_zstd_t *zstd = (ostream_zstd_t *)base;
	ZSTD_EndDirective op;
	ZSTD_outBuffer out;
	ZSTD_inBuffer in;
	size_t ret;

	op = finish ? ZSTD_e_end : ZSTD_e_continue;

	do {
		memset(&in, 0, sizeof(in));
		memset(&out, 0, sizeof(out));

		in.src = base->inbuf;
		in.size = base->inbuf_used;

		out.dst = base->outbuf;
		out.size = BUFSZ;

		ret = ZSTD_compressStream2(zstd->strm, &out, &in, op);

		if (ZSTD_isError(ret)) {
			fprintf(stderr, "%s: error in zstd compressor.\n",
				base->wrapped->get_filename(base->wrapped));
			return -1;
		}

		if (base->wrapped->append(base->wrapped, base->outbuf,
					  out.pos)) {
			return -1;
		}

		if (in.pos < in.size) {
			base->inbuf_used = in.size - in.pos;

			memmove(base->inbuf, base->inbuf + in.pos,
				base->inbuf_used);
		} else {
			base->inbuf_used = 0;
		}
	} while (finish && ret != 0);

	return 0;
}

static void cleanup(ostream_comp_t *base)
{
	ostream_zstd_t *zstd = (ostream_zstd_t *)base;

	ZSTD_freeCStream(zstd->strm);
}

ostream_comp_t *ostream_zstd_create(const char *filename)
{
	ostream_zstd_t *zstd = calloc(1, sizeof(*zstd));
	ostream_comp_t *base = (ostream_comp_t *)zstd;

	if (zstd == NULL) {
		fprintf(stderr, "%s: creating zstd wrapper: %s.\n",
			filename, strerror(errno));
		return NULL;
	}

	zstd->strm = ZSTD_createCStream();
	if (zstd->strm == NULL) {
		fprintf(stderr, "%s: error creating zstd decoder.\n",
			filename);
		free(zstd);
		return NULL;
	}

	base->flush_inbuf = flush_inbuf;
	base->cleanup = cleanup;
	return base;
}
#endif /* HAVE_ZSTD_STREAM */
