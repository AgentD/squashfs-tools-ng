/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <lzma.h>

#include "internal.h"

typedef struct {
	compressor_t base;
	size_t block_size;
} xz_compressor_t;

static int xz_write_options(compressor_t *base, int fd)
{
	(void)base;
	(void)fd;
	return 0;
}

static int xz_read_options(compressor_t *base, int fd)
{
	(void)base;
	(void)fd;
	fputs("xz extra options are not yet implemented\n", stderr);
	return -1;
}

static ssize_t xz_comp_block(compressor_t *base, const uint8_t *in,
			       size_t size, uint8_t *out, size_t outsize)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	lzma_filter filters[5];
	lzma_options_lzma opt;
	size_t written = 0;
	lzma_ret ret;

	if (lzma_lzma_preset(&opt, LZMA_PRESET_DEFAULT)) {
		fputs("error initializing xz options\n", stderr);
		return -1;
	}

	opt.dict_size = xz->block_size;

	filters[0].id = LZMA_FILTER_LZMA2;
	filters[0].options = &opt;

	filters[1].id = LZMA_VLI_UNKNOWN;
	filters[1].options = NULL;

	ret = lzma_stream_buffer_encode(filters, LZMA_CHECK_CRC32, NULL,
					in, size, out, &written, outsize);

	if (ret == LZMA_OK)
		return (written >= size) ? 0 : written;

	if (ret != LZMA_BUF_ERROR) {
		fputs("xz block compress failed\n", stderr);
		return -1;
	}

	return 0;
}

static ssize_t xz_uncomp_block(compressor_t *base, const uint8_t *in,
			       size_t size, uint8_t *out, size_t outsize)
{
	uint64_t memlimit = 32 * 1024 * 1024;
	size_t dest_pos = 0;
	size_t src_pos = 0;
	lzma_ret ret;
	(void)base;

	ret = lzma_stream_buffer_decode(&memlimit, 0, NULL,
					in, &src_pos, size,
					out, &dest_pos, outsize);

	if (ret == LZMA_OK && size == src_pos)
		return (ssize_t)dest_pos;

	fputs("xz block extract failed\n", stderr);
	return -1;
}

static void xz_destroy(compressor_t *base)
{
	free(base);
}

compressor_t *create_xz_compressor(bool compress, size_t block_size,
				   char *options)
{
	xz_compressor_t *xz = calloc(1, sizeof(*xz));
	compressor_t *base = (compressor_t *)xz;

	if (xz == NULL) {
		perror("creating xz compressor");
		return NULL;
	}

	xz->block_size = block_size;
	base->destroy = xz_destroy;
	base->do_block = compress ? xz_comp_block : xz_uncomp_block;
	base->write_options = xz_write_options;
	base->read_options = xz_read_options;
	return base;
}

void compressor_xz_print_help(void)
{
}
