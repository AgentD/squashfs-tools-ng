/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * lz4.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lz4.h>
#include <lz4hc.h>

#include "internal.h"

typedef struct {
	compressor_t base;
	bool high_compression;
} lz4_compressor_t;

typedef struct {
	uint32_t version;
	uint32_t flags;
} lz4_options;

#define LZ4LEGACY 1

static int lz4_write_options(compressor_t *base, int fd)
{
	lz4_compressor_t *lz4 = (lz4_compressor_t *)base;
	lz4_options opt = {
		.version = htole32(LZ4LEGACY),
		.flags = htole32(lz4->high_compression ?
				 SQFS_COMP_FLAG_LZ4_HC : 0),
	};

	return generic_write_options(fd, &opt, sizeof(opt));
}

static int lz4_read_options(compressor_t *base, int fd)
{
	lz4_options opt;
	(void)base;

	if (generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	opt.version = le32toh(opt.version);
	opt.flags = le32toh(opt.flags);

	if (opt.version != LZ4LEGACY) {
		fprintf(stderr, "unsupported lz4 version '%d'\n", opt.version);
		return -1;
	}

	return 0;
}

static ssize_t lz4_comp_block(compressor_t *base, const uint8_t *in,
			      size_t size, uint8_t *out, size_t outsize)
{
	lz4_compressor_t *lz4 = (lz4_compressor_t *)base;
	int ret;

	if (lz4->high_compression) {
		ret = LZ4_compress_HC((void *)in, (void *)out,
				      size, outsize, LZ4HC_CLEVEL_MAX);
	} else {
		ret = LZ4_compress_default((void *)in, (void *)out,
					   size, outsize);
	}

	if (ret < 0) {
		fputs("internal error in lz4 compressor\n", stderr);
		return -1;
	}

	return ret;
}

static ssize_t lz4_uncomp_block(compressor_t *base, const uint8_t *in,
				size_t size, uint8_t *out, size_t outsize)
{
	int ret;
	(void)base;

	ret = LZ4_decompress_safe((void *)in, (void *)out, size, outsize);

	if (ret < 0) {
		fputs("internal error in lz4 decompressor\n", stderr);
		return -1;
	}

	return ret;
}

static compressor_t *lz4_create_copy(compressor_t *cmp)
{
	lz4_compressor_t *lz4 = malloc(sizeof(*lz4));

	if (lz4 == NULL) {
		perror("creating additional lz4 compressor");
		return NULL;
	}

	memcpy(lz4, cmp, sizeof(*lz4));
	return (compressor_t *)lz4;
}

static void lz4_destroy(compressor_t *base)
{
	free(base);
}

compressor_t *create_lz4_compressor(const compressor_config_t *cfg)
{
	lz4_compressor_t *lz4 = calloc(1, sizeof(*lz4));
	compressor_t *base = (compressor_t *)lz4;

	if (cfg->flags & ~(SQFS_COMP_FLAG_LZ4_ALL |
			   SQFS_COMP_FLAG_GENERIC_ALL)) {
		fputs("creating lz4 compressor: unknown compressor flags\n",
		      stderr);
	}

	lz4 = calloc(1, sizeof(*lz4));
	base = (compressor_t *)lz4;
	if (lz4 == NULL) {
		perror("creating lz4 compressor");
		return NULL;
	}

	lz4->high_compression = (cfg->flags & SQFS_COMP_FLAG_LZ4_HC) != 0;

	base->destroy = lz4_destroy;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		lz4_uncomp_block : lz4_comp_block;
	base->write_options = lz4_write_options;
	base->read_options = lz4_read_options;
	base->create_copy = lz4_create_copy;
	return base;
}
