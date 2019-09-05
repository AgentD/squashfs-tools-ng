/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xz.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <lzma.h>

#include "internal.h"

typedef struct {
	sqfs_compressor_t base;
	size_t block_size;
	size_t dict_size;
	int flags;
} xz_compressor_t;

typedef struct {
	uint32_t dict_size;
	uint32_t flags;
} xz_options_t;

static bool is_dict_size_valid(size_t size)
{
	size_t x = size & (size - 1);

	if (x == 0)
		return true;

	return size == (x | (x >> 1));
}

static int xz_write_options(sqfs_compressor_t *base, int fd)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	xz_options_t opt;

	if (xz->flags == 0 && xz->dict_size == xz->block_size)
		return 0;

	opt.dict_size = htole32(xz->dict_size);
	opt.flags = htole32(xz->flags);

	return sqfs_generic_write_options(fd, &opt, sizeof(opt));
}

static int xz_read_options(sqfs_compressor_t *base, int fd)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	xz_options_t opt;

	if (sqfs_generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	opt.dict_size = le32toh(opt.dict_size);
	opt.flags = le32toh(opt.flags);

	if (!is_dict_size_valid(opt.dict_size)) {
		fputs("Invalid lzma dictionary size.\n", stderr);
		return -1;
	}

	if (opt.flags & ~SQFS_COMP_FLAG_XZ_ALL) {
		fputs("Unknown BCJ filter used.\n", stderr);
		return -1;
	}

	xz->flags = opt.flags;
	xz->dict_size = opt.dict_size;
	return 0;
}

static ssize_t compress(xz_compressor_t *xz, lzma_vli filter,
			const uint8_t *in, size_t size,
			uint8_t *out, size_t outsize)
{
	lzma_filter filters[5];
	lzma_options_lzma opt;
	size_t written = 0;
	lzma_ret ret;
	int i = 0;

	if (lzma_lzma_preset(&opt, LZMA_PRESET_DEFAULT)) {
		fputs("error initializing xz options\n", stderr);
		return -1;
	}

	opt.dict_size = xz->dict_size;

	if (filter != LZMA_VLI_UNKNOWN) {
		filters[i].id = filter;
		filters[i].options = NULL;
		++i;
	}

	filters[i].id = LZMA_FILTER_LZMA2;
	filters[i].options = &opt;
	++i;

	filters[i].id = LZMA_VLI_UNKNOWN;
	filters[i].options = NULL;
	++i;

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

static lzma_vli flag_to_vli(int flag)
{
	switch (flag) {
	case SQFS_COMP_FLAG_XZ_X86:
		return LZMA_FILTER_X86;
	case SQFS_COMP_FLAG_XZ_POWERPC:
		return LZMA_FILTER_POWERPC;
	case SQFS_COMP_FLAG_XZ_IA64:
		return LZMA_FILTER_IA64;
	case SQFS_COMP_FLAG_XZ_ARM:
		return LZMA_FILTER_ARM;
	case SQFS_COMP_FLAG_XZ_ARMTHUMB:
		return LZMA_FILTER_ARMTHUMB;
	case SQFS_COMP_FLAG_XZ_SPARC:
		return LZMA_FILTER_SPARC;
	}

	return LZMA_VLI_UNKNOWN;
}

static ssize_t xz_comp_block(sqfs_compressor_t *base, const uint8_t *in,
			     size_t size, uint8_t *out, size_t outsize)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	lzma_vli filter, selected = LZMA_VLI_UNKNOWN;
	size_t i, smallest;
	ssize_t ret;

	ret = compress(xz, LZMA_VLI_UNKNOWN, in, size, out, outsize);
	if (ret < 0 || xz->flags == 0)
		return ret;

	smallest = ret;

	for (i = 0; i & SQFS_COMP_FLAG_XZ_ALL; i <<= 1) {
		if ((xz->flags & i) == 0)
			continue;

		filter = flag_to_vli(i);

		ret = compress(xz, filter, in, size, out, outsize);
		if (ret < 0)
			return -1;

		if (ret > 0 && (smallest == 0 || (size_t)ret < smallest)) {
			smallest = ret;
			selected = filter;
		}
	}

	if (smallest == 0)
		return 0;

	return compress(xz, selected, in, size, out, outsize);
}

static ssize_t xz_uncomp_block(sqfs_compressor_t *base, const uint8_t *in,
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

static sqfs_compressor_t *xz_create_copy(sqfs_compressor_t *cmp)
{
	xz_compressor_t *xz = malloc(sizeof(*xz));

	if (xz == NULL) {
		perror("creating additional xz compressor");
		return NULL;
	}

	memcpy(xz, cmp, sizeof(*xz));
	return (sqfs_compressor_t *)xz;
}

static void xz_destroy(sqfs_compressor_t *base)
{
	free(base);
}

sqfs_compressor_t *xz_compressor_create(const sqfs_compressor_config_t *cfg)
{
	sqfs_compressor_t *base;
	xz_compressor_t *xz;

	if (cfg->flags & ~(SQFS_COMP_FLAG_GENERIC_ALL |
			   SQFS_COMP_FLAG_XZ_ALL)) {
		fputs("creating xz compressor: unknown compressor flags\n",
		      stderr);
		return NULL;
	}

	if (!is_dict_size_valid(cfg->opt.xz.dict_size)) {
		fputs("creating xz compressor: invalid dictionary size\n",
		      stderr);
		return NULL;
	}

	xz = calloc(1, sizeof(*xz));
	base = (sqfs_compressor_t *)xz;
	if (xz == NULL) {
		perror("creating xz compressor");
		return NULL;
	}

	xz->flags = cfg->flags;
	xz->dict_size = cfg->opt.xz.dict_size;
	xz->block_size = cfg->block_size;
	base->destroy = xz_destroy;
	base->do_block = (cfg->flags & SQFS_COMP_FLAG_UNCOMPRESS) ?
		xz_uncomp_block : xz_comp_block;
	base->write_options = xz_write_options;
	base->read_options = xz_read_options;
	base->create_copy = xz_create_copy;
	return base;
}
