/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * zstd.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zstd.h>

#include "xfrm/stream.h"
#include "xfrm/compress.h"

typedef struct {
	xfrm_stream_t base;

	ZSTD_CStream *cstrm;
	ZSTD_DStream *dstrm;
	bool compress;
} xfrm_zstd_t;

static const ZSTD_EndDirective zstd_action[] = {
	[XFRM_STREAM_FLUSH_NONE] = ZSTD_e_continue,
	[XFRM_STREAM_FLUSH_SYNC] = ZSTD_e_flush,
	[XFRM_STREAM_FLUSH_FULL] = ZSTD_e_end,
};

static int process_data(xfrm_stream_t *stream, const void *in,
			sqfs_u32 in_size, void *out, sqfs_u32 out_size,
			sqfs_u32 *in_read, sqfs_u32 *out_written,
			int flush_mode)
{
	xfrm_zstd_t *zstd = (xfrm_zstd_t *)stream;
	ZSTD_outBuffer out_desc;
	ZSTD_inBuffer in_desc;
	size_t ret;

	if (flush_mode < 0 || flush_mode >= XFRM_STREAM_FLUSH_COUNT)
		flush_mode = XFRM_STREAM_FLUSH_NONE;

	while (in_size > 0 && out_size > 0) {
		memset(&in_desc, 0, sizeof(in_desc));
		in_desc.src = in;
		in_desc.size = in_size;

		memset(&out_desc, 0, sizeof(out_desc));
		out_desc.dst = out;
		out_desc.size = out_size;

		if (zstd->compress) {
			ret = ZSTD_compressStream2(zstd->cstrm, &out_desc,
						   &in_desc,
						   zstd_action[flush_mode]);
		} else {
			ret = ZSTD_decompressStream(zstd->dstrm, &out_desc,
						    &in_desc);
		}

		if (ZSTD_isError(ret))
			return XFRM_STREAM_ERROR;

		in = (const char *)in + in_desc.pos;
		in_size -= in_desc.pos;
		*in_read += in_desc.pos;

		out = (char *)out + out_desc.pos;
		out_size -= out_desc.pos;
		*out_written += out_desc.pos;
	}

	if (flush_mode != XFRM_STREAM_FLUSH_NONE) {
		if (in_size == 0)
			return XFRM_STREAM_END;
	}

	if (in_size > 0 && out_size == 0)
		return XFRM_STREAM_BUFFER_FULL;

	return XFRM_STREAM_OK;
}

static void destroy(sqfs_object_t *obj)
{
	xfrm_zstd_t *zstd = (xfrm_zstd_t *)obj;

	if (zstd->compress) {
		ZSTD_freeCStream(zstd->cstrm);
	} else {
		ZSTD_freeDStream(zstd->dstrm);
	}

	free(zstd);
}

static xfrm_stream_t *stream_create(const compressor_config_t *cfg,
				    bool compress)
{
	xfrm_zstd_t *zstd = calloc(1, sizeof(*zstd));
	xfrm_stream_t *strm = (xfrm_stream_t *)zstd;
	(void)cfg;

	if (zstd == NULL) {
		perror("creating zstd stream compressor");
		return NULL;
	}

	if (compress) {
		zstd->cstrm = ZSTD_createCStream();
		if (zstd->cstrm == NULL)
			goto fail_strm;
	} else {
		zstd->dstrm = ZSTD_createDStream();
		if (zstd->dstrm == NULL)
			goto fail_strm;
	}

	zstd->compress = compress;
	strm->process_data = process_data;
	sqfs_object_init(zstd, destroy, NULL);
	return strm;
fail_strm:
	fputs("error initializing zstd stream.\n", stderr);
	free(zstd);
	return NULL;
}

xfrm_stream_t *compressor_stream_zstd_create(const compressor_config_t *cfg)
{
	return stream_create(cfg, true);
}

xfrm_stream_t *decompressor_stream_zstd_create(void)
{
	return stream_create(NULL, false);
}
