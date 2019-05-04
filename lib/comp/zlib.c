/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

#include "internal.h"

typedef struct {
	compressor_t base;

	z_stream strm;
	bool compress;

	size_t block_size;
} zlib_compressor_t;

static void zlib_destroy(compressor_t *base)
{
	zlib_compressor_t *zlib = (zlib_compressor_t *)base;

	if (zlib->compress) {
		deflateEnd(&zlib->strm);
	} else {
		inflateEnd(&zlib->strm);
	}

	free(zlib);
}

static ssize_t zlib_do_block(compressor_t *base, const uint8_t *in,
			     size_t size, uint8_t *out, size_t outsize)
{
	zlib_compressor_t *zlib = (zlib_compressor_t *)base;
	size_t written;
	int ret;

	if (zlib->compress) {
		ret = deflateReset(&zlib->strm);
	} else {
		ret = inflateReset(&zlib->strm);
	}

	if (ret != Z_OK) {
		fputs("resetting zlib stream failed\n", stderr);
		return -1;
	}

	zlib->strm.next_in = (void *)in;
	zlib->strm.avail_in = size;
	zlib->strm.next_out = out;
	zlib->strm.avail_out = outsize;

	if (zlib->compress) {
		ret = deflate(&zlib->strm, Z_FINISH);
	} else {
		ret = inflate(&zlib->strm, Z_FINISH);
	}

	if (ret == Z_STREAM_END) {
		written = zlib->strm.total_out;

		if (zlib->compress && written >= size)
			return 0;

		return (ssize_t)written;
	}

	if (ret != Z_OK) {
		fputs("zlib block processing failed\n", stderr);
		return -1;
	}

	return 0;
}

compressor_t *create_zlib_compressor(bool compress, size_t block_size)
{
	zlib_compressor_t *zlib = calloc(1, sizeof(*zlib));
	compressor_t *base = (compressor_t *)zlib;
	int ret;

	if (zlib == NULL) {
		perror("creating zlib stream");
		return NULL;
	}

	zlib->compress = compress;
	zlib->block_size = block_size;
	base->do_block = zlib_do_block;
	base->destroy = zlib_destroy;

	if (compress) {
		ret = deflateInit(&zlib->strm, Z_BEST_COMPRESSION);
	} else {
		ret = inflateInit(&zlib->strm);
	}

	if (ret != Z_OK) {
		fputs("internal error creating zlib stream\n", stderr);
		free(zlib);
		return NULL;
	}

	return base;
}
