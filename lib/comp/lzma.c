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
	uint8_t buffer[];
} lzma_compressor_t;

static ssize_t lzma_comp_block(compressor_t *base, uint8_t *block, size_t size)
{
	lzma_compressor_t *lzma = (lzma_compressor_t *)base;
	lzma_filter filters[5];
	lzma_options_lzma opt;
	size_t written = 0;
	lzma_ret ret;

	if (lzma_lzma_preset(&opt, LZMA_PRESET_DEFAULT)) {
		fputs("error initializing LZMA options\n", stderr);
		return -1;
	}

	opt.dict_size = lzma->block_size;

	filters[0].id = LZMA_FILTER_LZMA2;
	filters[0].options = &opt;

	filters[1].id = LZMA_VLI_UNKNOWN;
	filters[1].options = NULL;

	ret = lzma_stream_buffer_encode(filters, LZMA_CHECK_CRC32, NULL,
					block, size, lzma->buffer,
					&written, lzma->block_size);

	if (ret == LZMA_OK) {
		if (written >= size)
			return 0;

		memcpy(block, lzma->buffer, size);
		return written;
	}

	if (ret != LZMA_BUF_ERROR) {
		fputs("lzma block compress failed\n", stderr);
		return -1;
	}

	return 0;
}

static ssize_t lzma_uncomp_block(compressor_t *base, uint8_t *block,
				 size_t size)
{
	lzma_compressor_t *lzma = (lzma_compressor_t *)base;
	uint64_t memlimit = 32 * 1024 * 1024;
	size_t dest_pos = 0;
	size_t src_pos = 0;
	lzma_ret ret;

	ret = lzma_stream_buffer_decode(&memlimit, 0, NULL,
					block, &src_pos, size,
					lzma->buffer, &dest_pos,
					lzma->block_size);

	if (ret == LZMA_OK && size == src_pos) {
		memcpy(block, lzma->buffer, dest_pos);
		return (ssize_t)dest_pos;
	}

	fputs("lzma block extract failed\n", stderr);
	return -1;
}

static void lzma_destroy(compressor_t *base)
{
	free(base);
}

compressor_t *create_lzma_compressor(bool compress, size_t block_size)
{
	lzma_compressor_t *lzma = calloc(1, sizeof(*lzma) + block_size);
	compressor_t *base = (compressor_t *)lzma;

	if (lzma == NULL) {
		perror("creating lzma stream");
		return NULL;
	}

	lzma->block_size = block_size;
	base->destroy = lzma_destroy;
	base->do_block = compress ? lzma_comp_block : lzma_uncomp_block;
	return base;
}
