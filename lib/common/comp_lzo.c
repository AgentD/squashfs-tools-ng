/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * comp_lzo.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compress_cli.h"
#include "compat.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <lzo/lzo1x.h>

#define LZO_MAX_SIZE(size) (size + (size / 16) + 64 + 3)

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
	size_t block_size;
	int algorithm;
	int level;

	size_t buf_size;
	size_t work_size;

	sqfs_u8 buffer[];
} lzo_compressor_t;

typedef struct {
	sqfs_u32 algorithm;
	sqfs_u32 level;
} lzo_options_t;

static int lzo_write_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	sqfs_u8 buffer[sizeof(lzo_options_t) + 2];
	lzo_options_t opt;
	sqfs_u16 header;
	int ret;

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

	header = htole16(0x8000 | sizeof(opt));

	memcpy(buffer, &header, sizeof(header));
	memcpy(buffer + 2, &opt, sizeof(opt));

	ret = file->write_at(file, sizeof(sqfs_super_t),
			     buffer, sizeof(buffer));

	return ret ? ret : (int)sizeof(buffer);
}

static int lzo_read_options(sqfs_compressor_t *base, sqfs_file_t *file)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	sqfs_u8 buffer[sizeof(lzo_options_t) + 2];
	lzo_options_t opt;
	sqfs_u16 header;
	int ret;

	ret = file->read_at(file, sizeof(sqfs_super_t),
			    buffer, sizeof(buffer));
	if (ret)
		return ret;

	memcpy(&header, buffer, sizeof(header));
	if (le16toh(header) != (0x8000 | sizeof(opt)))
		return SQFS_ERROR_CORRUPTED;

	memcpy(&opt, buffer + 2, sizeof(opt));
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
	void *scratch;
	lzo_uint len;
	int ret;

	if (size >= 0x7FFFFFFF)
		return 0;

	scratch = lzo->buffer + lzo->work_size;
	len = lzo->buf_size - lzo->work_size;

	if (lzo->algorithm == SQFS_LZO1X_999 &&
	    lzo->level != SQFS_LZO_DEFAULT_LEVEL) {
		ret = lzo1x_999_compress_level(in, size, scratch, &len,
					       lzo->buffer, NULL, 0, 0,
					       lzo->level);
	} else {
		ret = lzo_algs[lzo->algorithm].compress(in, size, scratch,
							&len, lzo->buffer);
	}

	if (ret != LZO_E_OK)
		return SQFS_ERROR_COMPRESSOR;

	if (len < size && len <= outsize) {
		memcpy(out, scratch, len);
		return len;
	}

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

static void lzo_get_configuration(const sqfs_compressor_t *base,
				  sqfs_compressor_config_t *cfg)
{
	const lzo_compressor_t *lzo = (const lzo_compressor_t *)base;

	memset(cfg, 0, sizeof(*cfg));
	cfg->id = SQFS_COMP_LZO;
	cfg->block_size = lzo->block_size;

	cfg->opt.lzo.algorithm = lzo->algorithm;
	cfg->level = lzo->level;

	if (base->do_block == lzo_uncomp_block)
		cfg->flags |= SQFS_COMP_FLAG_UNCOMPRESS;
}

static sqfs_object_t *lzo_create_copy(const sqfs_object_t *cmp)
{
	const lzo_compressor_t *other = (const lzo_compressor_t *)cmp;
	lzo_compressor_t *lzo;

	lzo = calloc(1, sizeof(*lzo) + other->buf_size);
	if (lzo == NULL)
		return NULL;

	memcpy(lzo, other, sizeof(*lzo));
	return (sqfs_object_t *)lzo;
}

static void lzo_destroy(sqfs_object_t *base)
{
	free(base);
}

int lzo_compressor_create(const sqfs_compressor_config_t *cfg,
			  sqfs_compressor_t **out)
{
	sqfs_compressor_t *base;
	lzo_compressor_t *lzo;
	size_t scratch_size;

	if (cfg->flags & ~SQFS_COMP_FLAG_GENERIC_ALL)
		return SQFS_ERROR_UNSUPPORTED;

	if (cfg->opt.lzo.algorithm >= LZO_NUM_ALGS ||
	    lzo_algs[cfg->opt.lzo.algorithm].compress == NULL) {
		return SQFS_ERROR_UNSUPPORTED;
	}

	if (cfg->opt.lzo.algorithm == SQFS_LZO1X_999) {
		if (cfg->level > SQFS_LZO_MAX_LEVEL)
			return SQFS_ERROR_UNSUPPORTED;
	} else if (cfg->level != 0) {
		return SQFS_ERROR_UNSUPPORTED;
	}

	/* XXX: liblzo does not do bounds checking internally,
	   we need our own internal scratch buffer at worst case size... */
	if (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) {
		scratch_size = 0;
	} else {
		scratch_size = cfg->block_size;
		if (scratch_size < SQFS_META_BLOCK_SIZE)
			scratch_size = SQFS_META_BLOCK_SIZE;

		scratch_size = LZO_MAX_SIZE(scratch_size);
	}

	/* ...in addition to the LZO work space buffer of course */
	scratch_size += lzo_algs[cfg->opt.lzo.algorithm].bufsize;

	lzo = calloc(1, sizeof(*lzo) + scratch_size);
	base = (sqfs_compressor_t *)lzo;

	if (lzo == NULL)
		return SQFS_ERROR_ALLOC;

	lzo->block_size = cfg->block_size;
	lzo->algorithm = cfg->opt.lzo.algorithm;
	lzo->level = cfg->level;
	lzo->buf_size = scratch_size;
	lzo->work_size = lzo_algs[cfg->opt.lzo.algorithm].bufsize;

	base->get_configuration = lzo_get_configuration;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		lzo_uncomp_block : lzo_comp_block;
	base->write_options = lzo_write_options;
	base->read_options = lzo_read_options;
	((sqfs_object_t *)base)->copy = lzo_create_copy;
	((sqfs_object_t *)base)->destroy = lzo_destroy;

	*out = base;
	return 0;
}
