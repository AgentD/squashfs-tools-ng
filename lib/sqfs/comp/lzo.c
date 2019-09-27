/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * lzo.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lzo/lzo1x.h>

#include "internal.h"

#define LZO_NUM_ALGS (sizeof(lzo_algs) / sizeof(lzo_algs[0]))

typedef int (*lzo_cb_t)(const lzo_bytep src, lzo_uint src_len, lzo_bytep dst,
			lzo_uintp dst_len, lzo_voidp wrkmem);

static const struct {
	lzo_cb_t compress;
	size_t bufsize;
} lzo_algs[] = {
	[SQFS_LZO1X_1] = {
		.compress = lzo1x_1_compress,
		.bufsize = LZO1X_1_MEM_COMPRESS,
	},
	[SQFS_LZO1X_1_11] = {
		.compress = lzo1x_1_11_compress,
		.bufsize = LZO1X_1_11_MEM_COMPRESS,
	},
	[SQFS_LZO1X_1_12] = {
		.compress = lzo1x_1_12_compress,
		.bufsize = LZO1X_1_12_MEM_COMPRESS,
	},
	[SQFS_LZO1X_1_15] = {
		.compress = lzo1x_1_15_compress,
		.bufsize = LZO1X_1_15_MEM_COMPRESS,
	},
	[SQFS_LZO1X_999] = {
		.compress = lzo1x_999_compress,
		.bufsize = LZO1X_999_MEM_COMPRESS,
	},
};

typedef struct {
	sqfs_compressor_t base;
	int algorithm;
	int level;

	sqfs_u8 buffer[];
} lzo_compressor_t;

typedef struct {
	sqfs_u32 algorithm;
	sqfs_u32 level;
} lzo_options_t;

static int lzo_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_options_t opt;

	if (lzo->algorithm == SQFS_LZO_DEFAULT_ALG &&
	    lzo->level == SQFS_LZO_DEFAULT_LEVEL) {
		return 0;
	}

	opt.algorithm = htole32(lzo->algorithm);

	if (lzo->algorithm == SQFS_LZO1X_999) {
		opt.level = htole32(lzo->level);
	} else {
		opt.level = 0;
	}

	return sqfs_generic_write_options(file, &opt, sizeof(opt));
}

static int lzo_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_options_t opt;
	int ret;

	ret = sqfs_generic_read_options(file, &opt, sizeof(opt));
	if (ret)
		return ret;

	lzo->algorithm = le32toh(opt.algorithm);
	lzo->level = le32toh(opt.level);

	switch(lzo->algorithm) {
	case SQFS_LZO1X_1:
	case SQFS_LZO1X_1_11:
	case SQFS_LZO1X_1_12:
	case SQFS_LZO1X_1_15:
		if (lzo->level != 0)
			return SQFS_ERROR_UNSUPPORTED;
		break;
	case SQFS_LZO1X_999:
		if (lzo->level < 1 || lzo->level > 9)
			return SQFS_ERROR_UNSUPPORTED;
		break;
	default:
		return SQFS_ERROR_UNSUPPORTED;
	}

	return 0;
}

static sqfs_s32 lzo_comp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
			       sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_uint len = outsize;
	int ret;

	if (size >= 0x7FFFFFFF)
		return 0;

	if (lzo->algorithm == SQFS_LZO1X_999 &&
	    lzo->level != SQFS_LZO_DEFAULT_LEVEL) {
		ret = lzo1x_999_compress_level(in, size, out, &len,
					       lzo->buffer, NULL, 0, 0,
					       lzo->level);
	} else {
		ret = lzo_algs[lzo->algorithm].compress(in, size, out,
							&len, lzo->buffer);
	}

	if (ret != LZO_E_OK)
		return SQFS_ERROR_COMPRESSOR;

	if (len < size)
		return len;

	return 0;
}

static sqfs_s32 lzo_uncomp_block(sqfs_compressor_t *base, const sqfs_u8 *in,
				 sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_uint len = outsize;
	int ret;

	if (outsize >= 0x7FFFFFFF)
		return 0;

	ret = lzo1x_decompress_safe(in, size, out, &len, lzo->buffer);

	if (ret != LZO_E_OK)
		return SQFS_ERROR_COMPRESSOR;

	return len;
}

static sqfs_compressor_t *lzo_create_copy(sqfs_compressor_t *cmp)
{
	lzo_compressor_t *other = (lzo_compressor_t *)cmp;
	lzo_compressor_t *lzo;

	lzo = alloc_flex(sizeof(*lzo), 1, lzo_algs[other->algorithm].bufsize);
	if (lzo == NULL)
		return NULL;

	memcpy(lzo, other, sizeof(*lzo));
	return (sqfs_compressor_t *)lzo;
}

static void lzo_destroy(sqfs_compressor_t *base)
{
	free(base);
}

sqfs_compressor_t *lzo_compressor_create(const sqfs_compressor_config_t *cfg)
{
	sqfs_compressor_t *base;
	lzo_compressor_t *lzo;

	if (cfg->flags & ~SQFS_COMP_FLAG_GENERIC_ALL)
		return NULL;

	if (cfg->opt.lzo.algorithm > LZO_NUM_ALGS ||
	    lzo_algs[cfg->opt.lzo.algorithm].compress == NULL) {
		return NULL;
	}

	if (cfg->opt.lzo.algorithm == SQFS_LZO1X_999) {
		if (cfg->opt.lzo.level > SQFS_LZO_MAX_LEVEL)
			return NULL;
	} else if (cfg->opt.lzo.level != 0) {
		return NULL;
	}

	lzo = alloc_flex(sizeof(*lzo), 1,
			 lzo_algs[cfg->opt.lzo.algorithm].bufsize);
	base = (sqfs_compressor_t *)lzo;

	if (lzo == NULL)
		return NULL;

	lzo->algorithm = cfg->opt.lzo.algorithm;
	lzo->level = cfg->opt.lzo.level;

	base->destroy = lzo_destroy;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		lzo_uncomp_block : lzo_comp_block;
	base->write_options = lzo_write_options;
	base->read_options = lzo_read_options;
	base->create_copy = lzo_create_copy;
	return base;
}
