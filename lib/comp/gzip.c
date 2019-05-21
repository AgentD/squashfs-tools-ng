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
} gzip_compressor_t;

static void gzip_destroy(compressor_t *base)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;

	if (gzip->compress) {
		deflateEnd(&gzip->strm);
	} else {
		inflateEnd(&gzip->strm);
	}

	free(gzip);
}

static int gzip_write_options(compressor_t *base, int fd)
{
	(void)base;
	(void)fd;
	return 0;
}

static int gzip_read_options(compressor_t *base, int fd)
{
	(void)base;
	(void)fd;
	fputs("gzip extra options are not yet implemented\n", stderr);
	return -1;
}

static ssize_t gzip_do_block(compressor_t *base, const uint8_t *in,
			     size_t size, uint8_t *out, size_t outsize)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;
	size_t written;
	int ret;

	if (gzip->compress) {
		ret = deflateReset(&gzip->strm);
	} else {
		ret = inflateReset(&gzip->strm);
	}

	if (ret != Z_OK) {
		fputs("resetting zlib stream failed\n", stderr);
		return -1;
	}

	gzip->strm.next_in = (void *)in;
	gzip->strm.avail_in = size;
	gzip->strm.next_out = out;
	gzip->strm.avail_out = outsize;

	if (gzip->compress) {
		ret = deflate(&gzip->strm, Z_FINISH);
	} else {
		ret = inflate(&gzip->strm, Z_FINISH);
	}

	if (ret == Z_STREAM_END) {
		written = gzip->strm.total_out;

		if (gzip->compress && written >= size)
			return 0;

		return (ssize_t)written;
	}

	if (ret != Z_OK) {
		fputs("gzip block processing failed\n", stderr);
		return -1;
	}

	return 0;
}

compressor_t *create_gzip_compressor(bool compress, size_t block_size,
				     char *options)
{
	gzip_compressor_t *gzip = calloc(1, sizeof(*gzip));
	compressor_t *base = (compressor_t *)gzip;
	int ret;

	if (gzip == NULL) {
		perror("creating gzip compressor");
		return NULL;
	}

	gzip->compress = compress;
	gzip->block_size = block_size;
	base->do_block = gzip_do_block;
	base->destroy = gzip_destroy;
	base->write_options = gzip_write_options;
	base->read_options = gzip_read_options;

	if (compress) {
		ret = deflateInit(&gzip->strm, Z_BEST_COMPRESSION);
	} else {
		ret = inflateInit(&gzip->strm);
	}

	if (ret != Z_OK) {
		fputs("internal error creating zlib stream\n", stderr);
		free(gzip);
		return NULL;
	}

	return base;
}

void compressor_gzip_print_help(void)
{
}
