/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * zstd.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <zstd.h>

#include "internal.h"

typedef struct {
	sqfs_compressor_t base;
	ZSTD_CCtx *zctx;
	int level;
} zstd_compressor_t;

typedef struct {
	uint32_t level;
} zstd_options_t;

static int zstd_write_options(sqfs_compressor_t *base, int fd)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;
	zstd_options_t opt;
	(void)fd;

	if (zstd->level == SQFS_ZSTD_DEFAULT_LEVEL)
		return 0;

	opt.level = htole32(zstd->level);
	return sqfs_generic_write_options(fd, &opt, sizeof(opt));
}

static int zstd_read_options(sqfs_compressor_t *base, int fd)
{
	zstd_options_t opt;
	(void)base;

	if (sqfs_generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	opt.level = le32toh(opt.level);
	return 0;
}

static ssize_t zstd_comp_block(sqfs_compressor_t *base, const uint8_t *in,
			       size_t size, uint8_t *out, size_t outsize)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;
	size_t ret;

	ret = ZSTD_compressCCtx(zstd->zctx, out, outsize, in, size,
				zstd->level);

	if (ZSTD_isError(ret)) {
		fprintf(stderr, "internal error in ZSTD compressor: %s\n",
			ZSTD_getErrorName(ret));
		return -1;
	}

	return ret < size ? ret : 0;
}

static ssize_t zstd_uncomp_block(sqfs_compressor_t *base, const uint8_t *in,
				 size_t size, uint8_t *out, size_t outsize)
{
	size_t ret;
	(void)base;

	ret = ZSTD_decompress(out, outsize, in, size);

	if (ZSTD_isError(ret)) {
		fprintf(stderr, "error uncompressing ZSTD compressed data: %s",
			ZSTD_getErrorName(ret));
		return -1;
	}

	return ret;
}

static sqfs_compressor_t *zstd_create_copy(sqfs_compressor_t *cmp)
{
	zstd_compressor_t *zstd = malloc(sizeof(*zstd));

	if (zstd == NULL) {
		perror("creating additional zstd compressor");
		return NULL;
	}

	memcpy(zstd, cmp, sizeof(*zstd));

	zstd->zctx = ZSTD_createCCtx();

	if (zstd->zctx == NULL) {
		fputs("error creating addtional zstd compression context\n",
		      stderr);
		free(zstd);
		return NULL;
	}

	return (sqfs_compressor_t *)zstd;
}

static void zstd_destroy(sqfs_compressor_t *base)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;

	ZSTD_freeCCtx(zstd->zctx);
	free(zstd);
}

sqfs_compressor_t *zstd_compressor_create(const sqfs_compressor_config_t *cfg)
{
	zstd_compressor_t *zstd;
	sqfs_compressor_t *base;

	if (cfg->flags & ~SQFS_COMP_FLAG_GENERIC_ALL) {
		fputs("creating zstd compressor: unknown compressor flags\n",
		      stderr);
	}

	if (cfg->opt.zstd.level < 1 ||
	    cfg->opt.zstd.level > ZSTD_maxCLevel()) {
		goto fail_level;
	}

	zstd = calloc(1, sizeof(*zstd));
	base = (sqfs_compressor_t *)zstd;
	if (zstd == NULL) {
		perror("creating zstd compressor");
		return NULL;
	}

	zstd->zctx = ZSTD_createCCtx();
	if (zstd->zctx == NULL) {
		fputs("error creating zstd compression context\n", stderr);
		free(zstd);
		return NULL;
	}

	base->destroy = zstd_destroy;
	base->do_block = cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS ?
		zstd_uncomp_block : zstd_comp_block;
	base->write_options = zstd_write_options;
	base->read_options = zstd_read_options;
	base->create_copy = zstd_create_copy;
	return base;
fail_level:
	fprintf(stderr, "zstd compression level must be a number in the range "
		"1...%d\n", ZSTD_maxCLevel());
	return NULL;
}
