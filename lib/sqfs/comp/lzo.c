/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * lzo.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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
	compressor_t base;
	int algorithm;
	int level;

	uint8_t buffer[];
} lzo_compressor_t;

typedef struct {
	uint32_t algorithm;
	uint32_t level;
} lzo_options_t;

static int lzo_write_options(compressor_t *base, int fd)
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

	return generic_write_options(fd, &opt, sizeof(opt));
}

static int lzo_read_options(compressor_t *base, int fd)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_options_t opt;

	if (generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	lzo->algorithm = le32toh(opt.algorithm);
	lzo->level = le32toh(opt.level);

	switch(lzo->algorithm) {
	case SQFS_LZO1X_1:
	case SQFS_LZO1X_1_11:
	case SQFS_LZO1X_1_12:
	case SQFS_LZO1X_1_15:
		if (lzo->level != 0)
			goto fail_level;
		break;
	case SQFS_LZO1X_999:
		if (lzo->level < 1 || lzo->level > 9)
			goto fail_level;
		break;
	default:
		fputs("Unsupported LZO variant specified.\n", stderr);
		return -1;
	}

	return 0;
fail_level:
	fputs("Unsupported LZO compression level specified.\n", stderr);
	return -1;
}

static ssize_t lzo_comp_block(compressor_t *base, const uint8_t *in,
			      size_t size, uint8_t *out, size_t outsize)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_uint len = outsize;
	int ret;

	if (lzo->algorithm == SQFS_LZO1X_999 &&
	    lzo->level != SQFS_LZO_DEFAULT_LEVEL) {
		ret = lzo1x_999_compress_level(in, size, out, &len,
					       lzo->buffer, NULL, 0, 0,
					       lzo->level);
	} else {
		ret = lzo_algs[lzo->algorithm].compress(in, size, out,
							&len, lzo->buffer);
	}

	if (ret != LZO_E_OK) {
		fputs("LZO compression failed.\n", stderr);
		return -1;
	}

	if (len < size)
		return len;

	return 0;
}

static ssize_t lzo_uncomp_block(compressor_t *base, const uint8_t *in,
				size_t size, uint8_t *out, size_t outsize)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_uint len = outsize;
	int ret;

	ret = lzo1x_decompress_safe(in, size, out, &len, lzo->buffer);

	if (ret != LZO_E_OK) {
		fputs("lzo decompress: input data is corrupted\n", stderr);
		return -1;
	}

	return len;
}

static compressor_t *lzo_create_copy(compressor_t *cmp)
{
	lzo_compressor_t *other = (lzo_compressor_t *)cmp;
	lzo_compressor_t *lzo;

	lzo = alloc_flex(sizeof(*lzo), 1, lzo_algs[other->algorithm].bufsize);

	if (lzo == NULL) {
		perror("creating additional lzo compressor");
		return NULL;
	}

	memcpy(lzo, other, sizeof(*lzo));
	return (compressor_t *)lzo;
}

static void lzo_destroy(compressor_t *base)
{
	free(base);
}

compressor_t *create_lzo_compressor(const compressor_config_t *cfg)
{
	lzo_compressor_t *lzo;
	compressor_t *base;

	if (cfg->flags & ~SQFS_COMP_FLAG_GENERIC_ALL) {
		fputs("creating lzo compressor: unknown compressor flags\n",
		      stderr);
		return NULL;
	}

	if (cfg->opt.lzo.algorithm > LZO_NUM_ALGS ||
	    lzo_algs[cfg->opt.lzo.algorithm].compress == NULL) {
		fputs("creating lzo compressor: unknown LZO variant\n",
		      stderr);
		return NULL;
	}

	if (cfg->opt.lzo.algorithm == SQFS_LZO1X_999) {
		if (cfg->opt.lzo.level > SQFS_LZO_MAX_LEVEL) {
			fputs("creating lzo compressor: compression level "
			      "must be between 0 and 9 inclusive\n", stderr);
			return NULL;
		}
	} else if (cfg->opt.lzo.level != 0) {
		fputs("creating lzo compressor: level argument "
		      "only supported by lzo1x 999\n", stderr);
		return NULL;
	}

	lzo = alloc_flex(sizeof(*lzo), 1,
			 lzo_algs[cfg->opt.lzo.algorithm].bufsize);
	base = (compressor_t *)lzo;

	if (lzo == NULL) {
		perror("creating lzo compressor");
		return NULL;
	}

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
