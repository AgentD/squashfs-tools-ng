/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <zstd.h>

#include "internal.h"

#define ZSTD_DEFAULT_COMPRESSION_LEVEL 15

typedef struct {
	compressor_t base;
	ZSTD_CCtx *zctx;
	int level;
} zstd_compressor_t;

typedef struct {
	uint32_t level;
} zstd_options_t;

static int zstd_write_options(compressor_t *base, int fd)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;
	zstd_options_t opt;
	(void)fd;

	if (zstd->level == ZSTD_DEFAULT_COMPRESSION_LEVEL)
		return 0;

	opt.level = htole32(zstd->level);
	return generic_write_options(fd, &opt, sizeof(opt));
}

static int zstd_read_options(compressor_t *base, int fd)
{
	zstd_options_t opt;
	(void)base;

	if (generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	opt.level = le32toh(opt.level);
	return 0;
}

static ssize_t zstd_comp_block(compressor_t *base, const uint8_t *in,
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

static ssize_t zstd_uncomp_block(compressor_t *base, const uint8_t *in,
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

static void zstd_destroy(compressor_t *base)
{
	zstd_compressor_t *zstd = (zstd_compressor_t *)base;

	ZSTD_freeCCtx(zstd->zctx);
	free(zstd);
}

compressor_t *create_zstd_compressor(bool compress, size_t block_size,
				     char *options)
{
	zstd_compressor_t *zstd = calloc(1, sizeof(*zstd));
	compressor_t *base = (compressor_t *)zstd;
	size_t i;
	(void)block_size;

	if (zstd == NULL) {
		perror("creating zstd compressor");
		return NULL;
	}

	zstd->level = ZSTD_DEFAULT_COMPRESSION_LEVEL;

	if (options != NULL) {
		if (strncmp(options, "level=", 6) == 0) {
			options += 6;

			for (i = 0; isdigit(options[i]); ++i)
				;

			if (i == 0 || options[i] != '\0' || i > 6)
				goto fail_level;

			zstd->level = atoi(options);

			if (zstd->level < 1 || zstd->level > ZSTD_maxCLevel())
				goto fail_level;
		} else {
			goto fail_opt;
		}
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
fail_level:
	fprintf(stderr, "zstd compression level must be a number in the range "
		"1...%d\n", ZSTD_maxCLevel());
	free(zstd);
	return NULL;
fail_opt:
	fputs("Unsupported extra options for zstd compressor\n", stderr);
	free(zstd);
	return NULL;
}

void compressor_zstd_print_help(void)
{
	printf("Available options for zstd compressor:\n"
	       "\n"
	       "    level=<value>    Set compression level. Defaults to %d.\n"
	       "                     Maximum is %d.\n"
	       "\n",
	       ZSTD_DEFAULT_COMPRESSION_LEVEL, ZSTD_maxCLevel());
}
