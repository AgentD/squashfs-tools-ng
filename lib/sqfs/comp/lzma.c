/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * lzma.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <lzma.h>

#include "internal.h"

#define LZMA_SIZE_OFFSET (5)
#define LZMA_SIZE_BYTES (8)
#define LZMA_HEADER_SIZE (13)

#define LZMA_DEFAULT_LEVEL (5)
#define MEMLIMIT (32 * 1024 * 1024)

typedef struct {
	sqfs_compressor_t base;
	size_t block_size;
} lzma_compressor_t;

static int lzma_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	(void)base; (void)file;
	return 0;
}

static int lzma_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	(void)base;
	(void)file;
	return SQFS_ERROR_UNSUPPORTED;
}

static sqfs_s32 lzma_comp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	lzma_compressor_t *lzma = (lzma_compressor_t *)base;
	lzma_stream strm = LZMA_STREAM_INIT;
	lzma_options_lzma opt;
	int ret;

	if (outsize < LZMA_HEADER_SIZE || size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	lzma_lzma_preset(&opt, LZMA_DEFAULT_LEVEL);
	opt.dict_size = lzma->block_size;

	if (lzma_alone_encoder(&strm, &opt) != LZMA_OK) {
		lzma_end(&strm);
		return SQFS_ERROR_COMPRESSOR;
	}

	strm.next_out = out;
	strm.avail_out = outsize;
	strm.next_in = in;
	strm.avail_in = size;

	ret = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);

	if (ret != LZMA_STREAM_END)
		return ret == LZMA_OK ? 0 : SQFS_ERROR_COMPRESSOR;

	if (strm.total_out > size)
		return 0;

	out[LZMA_SIZE_OFFSET    ] = size & 0xFF;
	out[LZMA_SIZE_OFFSET + 1] = (size >> 8) & 0xFF;
	out[LZMA_SIZE_OFFSET + 2] = (size >> 16) & 0xFF;
	out[LZMA_SIZE_OFFSET + 3] = (size >> 24) & 0xFF;
	out[LZMA_SIZE_OFFSET + 4] = 0;
	out[LZMA_SIZE_OFFSET + 5] = 0;
	out[LZMA_SIZE_OFFSET + 6] = 0;
	out[LZMA_SIZE_OFFSET + 7] = 0;
	return strm.total_out;
}

static sqfs_s32 lzma_uncomp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				  sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	sqfs_u8 lzma_header[LZMA_HEADER_SIZE];
	lzma_stream strm = LZMA_STREAM_INIT;
	size_t hdrsize;
	int ret;
	(void)base;

	if (size >= 0x7FFFFFFF)
		return SQFS_ERROR_ARG_INVALID;

	if (size < sizeof(lzma_header))
		return SQFS_ERROR_CORRUPTED;

	hdrsize = (size_t)in[LZMA_SIZE_OFFSET] |
		((size_t)in[LZMA_SIZE_OFFSET + 1] << 8) |
		((size_t)in[LZMA_SIZE_OFFSET + 2] << 16) |
		((size_t)in[LZMA_SIZE_OFFSET + 3] << 24);

	if (hdrsize > outsize)
		return 0;

	if (lzma_alone_decoder(&strm, MEMLIMIT) != LZMA_OK) {
		lzma_end(&strm);
		return SQFS_ERROR_COMPRESSOR;
	}

	memcpy(lzma_header, in, sizeof(lzma_header));
	memset(lzma_header + LZMA_SIZE_OFFSET, 0xFF, LZMA_SIZE_BYTES);

	strm.next_out = out;
	strm.avail_out = outsize;
	strm.next_in = lzma_header;
	strm.avail_in = sizeof(lzma_header);

	ret = lzma_code(&strm, LZMA_RUN);

	if (ret != LZMA_OK || strm.avail_in != 0) {
		lzma_end(&strm);
		return SQFS_ERROR_COMPRESSOR;
	}

	strm.next_in = in + sizeof(lzma_header);
	strm.avail_in = size - sizeof(lzma_header);

	ret = lzma_code(&strm, LZMA_FINISH);
	lzma_end(&strm);

	if (ret != LZMA_STREAM_END && ret != LZMA_OK)
		return SQFS_ERROR_COMPRESSOR;

	if (ret == LZMA_OK) {
		if (strm.total_out < hdrsize || strm.avail_in != 0)
			return 0;
	}

	return hdrsize;
}

static void lzma_get_configuration(const sqfs_compressor_t *base,
				   sqfs_compressor_config_t *cfg)
{
	const lzma_compressor_t *lzma = (const lzma_compressor_t *)base;

	memset(cfg, 0, sizeof(*cfg));
	cfg->id = SQFS_COMP_LZMA;
	cfg->block_size = lzma->block_size;

	if (base->do_block == lzma_uncomp_block)
		cfg->flags |= SQFS_COMP_FLAG_UNCOMPRESS;
}

static sqfs_compressor_t *lzma_create_copy(sqfs_compressor_t *cmp)
{
	lzma_compressor_t *copy = malloc(sizeof(*copy));

	if (copy != NULL)
		memcpy(copy, cmp, sizeof(*copy));

	return (sqfs_compressor_t *)copy;
}

static void lzma_destroy(sqfs_object_t *base)
{
	free(base);
}

sqfs_compressor_t *lzma_compressor_create(const sqfs_compressor_config_t *cfg)
{
	sqfs_compressor_t *base;
	lzma_compressor_t *lzma;

	if (cfg->flags & ~SQFS_COMP_FLAG_GENERIC_ALL)
		return NULL;

	lzma = calloc(1, sizeof(*lzma));
	base = (sqfs_compressor_t *)lzma;
	if (lzma == NULL)
		return NULL;

	lzma->block_size = cfg->block_size;

	if (lzma->block_size < SQFS_META_BLOCK_SIZE)
		lzma->block_size = SQFS_META_BLOCK_SIZE;

	base->get_configuration = lzma_get_configuration;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		lzma_uncomp_block : lzma_comp_block;
	base->write_options = lzma_write_options;
	base->read_options = lzma_read_options;
	base->create_copy = lzma_create_copy;
	((sqfs_object_t *)base)->destroy = lzma_destroy;
	return base;
}
