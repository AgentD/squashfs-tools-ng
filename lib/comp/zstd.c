/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <zstd.h>

#include "internal.h"

#define ZSTD_DEFAULT_COMPRESSION_LEVEL 15

typedef struct {
	compressor_t base;
	ZSTD_CCtx *zctx;
} zstd_compressor_t;

static int zstd_write_options(compressor_t *base, int fd)
{
	(void)base; (void)fd;
	return 0;
}

static int zstd_read_options(compressor_t *base, int fd)
{
	(void)base; (void)fd;
	fputs("zstd extra options are not yet implemented\n", stderr);
	return -1;
}

static ssize_t zstd_comp_block(compressor_t *base, const uint8_t *in,
			       size_t size, uint8_t *out, size_t outsize)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;
	size_t ret;

	ret = ZSTD_compressCCtx(zstd->zctx, out, outsize, in, size,
				ZSTD_DEFAULT_COMPRESSION_LEVEL);

	if (ZSTD_isError(ret)) {
		fputs("internal error in ZSTD compressor\n", stderr);
		return -1;
	}

	return ret < size ? ret : 0;
}

static ssize_t zstd_uncomp_block(compressor_t *base, const uint8_t *in,
				 size_t size, uint8_t *out, size_t outsize)
{
	size_t ret;
	(void)base;

	ret = ZSTD_decompress(out, outsize, in, size);

	if (ZSTD_isError(ret)) {
		fputs("error uncompressing ZSTD compressed data", stderr);
		return -1;
	}

	return ret;
}

static void zstd_destroy(compressor_t *base)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;

	ZSTD_freeCCtx(zstd->zctx);
	free(zstd);
}

compressor_t *create_zstd_compressor(bool compress, size_t block_size)
{
	zstd_compressor_t *zstd = calloc(1, sizeof(*zstd));
	compressor_t *base = (compressor_t *)zstd;
	(void)block_size;

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
	base->do_block = compress ? zstd_comp_block : zstd_uncomp_block;
	base->write_options = zstd_write_options;
	base->read_options = zstd_read_options;
	return base;
}
