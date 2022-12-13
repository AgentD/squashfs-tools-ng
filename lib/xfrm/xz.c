/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xz.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include <stdlib.h>
#include <stdio.h>
#include <lzma.h>

#include "xfrm/stream.h"
#include "xfrm/compress.h"

typedef struct {
	xfrm_stream_t base;

	lzma_stream strm;

	sqfs_u64 memlimit;
	lzma_filter filters[3];
	lzma_options_lzma opt;
	lzma_vli vli_filter;
	sqfs_u32 presets;

	bool compress;
	bool initialized;
} xfrm_xz_t;

static const lzma_action xzlib_action[] = {
	[XFRM_STREAM_FLUSH_NONE] = LZMA_RUN,
	[XFRM_STREAM_FLUSH_SYNC] = LZMA_FULL_FLUSH,
	[XFRM_STREAM_FLUSH_FULL] = LZMA_FINISH,
};

static lzma_vli vli_filter_from_flags(int vli)
{
	switch (vli) {
	case COMP_XZ_VLI_X86:
		return LZMA_FILTER_X86;
	case COMP_XZ_VLI_POWERPC:
		return LZMA_FILTER_POWERPC;
	case COMP_XZ_VLI_IA64:
		return LZMA_FILTER_IA64;
	case COMP_XZ_VLI_ARM:
		return LZMA_FILTER_ARM;
	case COMP_XZ_VLI_ARMTHUMB:
		return LZMA_FILTER_ARMTHUMB;
	case COMP_XZ_VLI_SPARC:
		return LZMA_FILTER_SPARC;
	default:
		return LZMA_VLI_UNKNOWN;
	}
}

static int process_data(xfrm_stream_t *stream, const void *in,
			sqfs_u32 in_size, void *out, sqfs_u32 out_size,
			sqfs_u32 *in_read, sqfs_u32 *out_written,
			int flush_mode)
{
	xfrm_xz_t *xz = (xfrm_xz_t *)stream;
	lzma_ret ret_xz;
	sqfs_u32 diff;

	if (!xz->initialized) {
		if (xz->compress) {
			ret_xz = lzma_stream_encoder(&xz->strm, xz->filters,
						     LZMA_CHECK_CRC32);
		} else {
			ret_xz = lzma_stream_decoder(&xz->strm,
						     xz->memlimit, 0);
		}

		if (ret_xz != LZMA_OK)
			return XFRM_STREAM_ERROR;

		xz->initialized = true;
	}

	if (flush_mode < 0 || flush_mode >= XFRM_STREAM_FLUSH_COUNT)
		flush_mode = XFRM_STREAM_FLUSH_NONE;

	while (in_size > 0 && out_size > 0) {
		xz->strm.next_in = in;
		xz->strm.avail_in = in_size;

		xz->strm.next_out = out;
		xz->strm.avail_out = out_size;

		ret_xz = lzma_code(&xz->strm, xzlib_action[flush_mode]);

		if (ret_xz != LZMA_OK && ret_xz != LZMA_BUF_ERROR &&
		    ret_xz != LZMA_STREAM_END) {
			return XFRM_STREAM_ERROR;
		}

		diff = in_size - xz->strm.avail_in;
		in = (const char *)in + diff;
		in_size -= diff;
		*in_read += diff;

		diff = out_size - xz->strm.avail_out;
		out = (char *)out + diff;
		out_size -= diff;
		*out_written += diff;

		if (ret_xz == LZMA_BUF_ERROR)
			return XFRM_STREAM_BUFFER_FULL;

		if (ret_xz == LZMA_STREAM_END) {
			lzma_end(&xz->strm);
			xz->initialized = false;
			return XFRM_STREAM_END;
		}
	}

	return XFRM_STREAM_OK;
}

static void destroy(sqfs_object_t *obj)
{
	xfrm_xz_t *xz = (xfrm_xz_t *)obj;

	if (xz->initialized)
		lzma_end(&xz->strm);

	free(xz);
}

static xfrm_stream_t *create_stream(const compressor_config_t *cfg,
				    bool compress)
{
	xfrm_xz_t *xz = calloc(1, sizeof(*xz));
	xfrm_stream_t *xfrm = (xfrm_stream_t *)xz;
	int i = 0;

	if (xz == NULL) {
		perror("creating xz stream compressor");
		return NULL;
	}

	xz->memlimit = 128 * 1024 * 1024;
	xz->compress = compress;
	xz->initialized = false;

	if (compress) {
		if (cfg == NULL) {
			xz->presets = COMP_XZ_DEFAULT_LEVEL;
		} else {
			xz->presets = cfg->level;
			if (cfg->flags & COMP_FLAG_XZ_EXTREME)
				xz->presets |= LZMA_PRESET_EXTREME;
		}

		if (lzma_lzma_preset(&xz->opt, xz->presets))
			goto fail_init;

		if (cfg == NULL) {
			xz->vli_filter = LZMA_VLI_UNKNOWN;
		} else {
			xz->vli_filter = vli_filter_from_flags(cfg->opt.xz.vli);
		}

		if (xz->vli_filter != LZMA_VLI_UNKNOWN) {
			xz->filters[i].id = xz->vli_filter;
			xz->filters[i].options = NULL;
			++i;
		}

		xz->filters[i].id = LZMA_FILTER_LZMA2;
		xz->filters[i].options = &xz->opt;
		++i;

		xz->filters[i].id = LZMA_VLI_UNKNOWN;
		xz->filters[i].options = NULL;
		++i;
	}

	xfrm->process_data = process_data;
	sqfs_object_init(xz, destroy, NULL);
	return xfrm;
fail_init:
	fputs("error initializing XZ compressor\n", stderr);
	free(xz);
	return NULL;
}

xfrm_stream_t *compressor_stream_xz_create(const compressor_config_t *cfg)
{
	return create_stream(cfg, true);
}

xfrm_stream_t *decompressor_stream_xz_create(void)
{
	return create_stream(NULL, false);
}
