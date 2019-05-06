/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <lzo/lzo1x.h>

#include "internal.h"

typedef struct {
	compressor_t base;

	uint8_t buffer[LZO1X_999_MEM_COMPRESS];
} lzo_compressor_t;

static ssize_t lzo_comp_block(compressor_t *base, const uint8_t *in,
			      size_t size, uint8_t *out, size_t outsize)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_uint len = outsize;

	if (lzo1x_999_compress(in, size, out, &len, lzo->buffer) != LZO_E_OK) {
		fputs("lzo1x_999 failed. According to its "
		      "manual, this shouldn't happen!\n", stderr);
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

static void lzo_destroy(compressor_t *base)
{
	free(base);
}

compressor_t *create_lzo_compressor(bool compress, size_t block_size)
{
	lzo_compressor_t *lzo = calloc(1, sizeof(*lzo));
	compressor_t *base = (compressor_t *)lzo;
	(void)block_size;

	if (lzo == NULL) {
		perror("creating lzo compressor");
		return NULL;
	}

	base->destroy = lzo_destroy;
	base->do_block = compress ? lzo_comp_block : lzo_uncomp_block;
	return base;
}
