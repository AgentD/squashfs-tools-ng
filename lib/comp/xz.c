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

typedef enum {
	XZ_FILTER_X86 = 0x01,
	XZ_FILTER_POWERPC = 0x02,
	XZ_FILTER_IA64 = 0x04,
	XZ_FILTER_ARM = 0x08,
	XZ_FILTER_ARMTHUMB = 0x10,
	XZ_FILTER_SPARC = 0x20,

	XZ_FILTER_ALL = 0x3F,
} XZ_FILTER_FLAG;

typedef struct {
	compressor_t base;
	size_t block_size;
	size_t dict_size;
	int flags;
} xz_compressor_t;

typedef struct {
	uint32_t dict_size;
	uint32_t flags;
} xz_options_t;

static const struct {
	const char *name;
	lzma_vli filter;
	int flag;
} xz_filters[] = {
	{ "x86", LZMA_FILTER_X86, XZ_FILTER_X86 },
	{ "powerpc", LZMA_FILTER_POWERPC, XZ_FILTER_POWERPC },
	{ "ia64", LZMA_FILTER_IA64, XZ_FILTER_IA64 },
	{ "arm", LZMA_FILTER_ARM, XZ_FILTER_ARM },
	{ "armthumb", LZMA_FILTER_ARMTHUMB, XZ_FILTER_ARMTHUMB },
	{ "sparc", LZMA_FILTER_SPARC, XZ_FILTER_SPARC },
};

#define XZ_NUM_FILTERS (sizeof(xz_filters) / sizeof(xz_filters[0]))

static int xz_write_options(compressor_t *base, int fd)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	xz_options_t opt;

	if (xz->flags == 0 && xz->dict_size == xz->block_size)
		return 0;

	opt.dict_size = htole32(xz->dict_size);
	opt.flags = htole32(xz->flags);

	return generic_write_options(fd, &opt, sizeof(opt));
}

static int xz_read_options(compressor_t *base, int fd)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	xz_options_t opt;
	uint32_t mask;

	if (generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	opt.dict_size = le32toh(opt.dict_size);
	opt.flags = le32toh(opt.flags);

	mask = opt.dict_size & (opt.dict_size - 1);

	if (mask != 0 && ((mask & (mask - 1)) != 0)) {
		fputs("Invalid lzma dictionary size.\n", stderr);
		return -1;
	}

	if (opt.flags & ~XZ_FILTER_ALL) {
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

static ssize_t xz_comp_block(compressor_t *base, const uint8_t *in,
			       size_t size, uint8_t *out, size_t outsize)
{
	xz_compressor_t *xz = (xz_compressor_t *)base;
	lzma_vli selected = LZMA_VLI_UNKNOWN;
	size_t i, smallest;
	ssize_t ret;

	ret = compress(xz, LZMA_VLI_UNKNOWN, in, size, out, outsize);
	if (ret < 0 || xz->flags == 0)
		return ret;

	smallest = ret;

	for (i = 0; i < XZ_NUM_FILTERS; ++i) {
		if (!(xz->flags & xz_filters[i].flag))
			continue;

		ret = compress(xz, xz_filters[i].filter, in, size, out, outsize);
		if (ret < 0)
			return -1;

		if (ret > 0 && (smallest == 0 || (size_t)ret < smallest)) {
			smallest = ret;
			selected = xz_filters[i].filter;
		}
	}

	if (smallest == 0)
		return 0;

	return compress(xz, selected, in, size, out, outsize);
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

static int process_options(char *options, size_t blocksize,
			   int *flags, uint64_t *dictsize)
{
	enum {
		OPT_DICT = 0,
	};
	char *const token[] = {
		[OPT_DICT] = (char *)"dictsize",
		NULL
	};
	char *subopts, *value;
	uint64_t mask;
	size_t i;
	int opt;

	subopts = options;

	while (*subopts != '\0') {
		opt = getsubopt(&subopts, token, &value);

		switch (opt) {
		case OPT_DICT:
			if (value == NULL)
				goto fail_value;

			for (i = 0; isdigit(value[i]); ++i)
				;

			if (i < 1 || i > 9)
				goto fail_dict;

			*dictsize = atol(value);

			switch (value[i]) {
			case '\0':
				break;
			case 'm':
			case 'M':
				*dictsize <<= 20;
				break;
			case 'k':
			case 'K':
				*dictsize <<= 10;
				break;
			case '%':
				*dictsize = ((*dictsize) * blocksize) / 100;
				break;
			default:
				goto fail_dict;
			}

			if (*dictsize > 0x0FFFFFFFFUL)
				goto fail_dict_ov;

			mask = *dictsize & (*dictsize - 1);

			if (mask != 0 && ((mask & (mask - 1)) != 0))
				goto fail_dict_pot;
			break;
		default:
			for (i = 0; i < XZ_NUM_FILTERS; ++i) {
				if (strcmp(value, xz_filters[i].name) == 0) {
					*flags |= xz_filters[i].flag;
					break;
				}
			}
			if (i == XZ_NUM_FILTERS)
				goto fail_opt;
			break;
		}
	}

	return 0;
fail_dict_pot:
	fputs("dictionary size must be either 2^n or 2^n + 2^(n-1)\n", stderr);
	return -1;
fail_dict_ov:
	fputs("dictionary size too large.\n", stderr);
	return -1;
fail_dict:
	fputs("dictionary size must be a number with the optional "
	      "suffix 'm','k' or '%'.\n", stderr);
	return -1;
fail_opt:
	fprintf(stderr, "Unknown option '%s'.\n", value);
	return -1;
fail_value:
	fprintf(stderr, "Missing value for '%s'.\n", token[opt]);
	return -1;
}

compressor_t *create_xz_compressor(bool compress, size_t block_size,
				   char *options)
{
	uint64_t dictsize = block_size;
	xz_compressor_t *xz;
	compressor_t *base;
	int flags = 0;

	if (options != NULL) {
		if (process_options(options, block_size, &flags, &dictsize))
			return NULL;
	}

	xz = calloc(1, sizeof(*xz));
	base = (compressor_t *)xz;
	if (xz == NULL) {
		perror("creating xz compressor");
		return NULL;
	}

	xz->flags = flags;
	xz->dict_size = dictsize;
	xz->block_size = block_size;
	base->destroy = xz_destroy;
	base->do_block = compress ? xz_comp_block : xz_uncomp_block;
	base->write_options = xz_write_options;
	base->read_options = xz_read_options;
	return base;
}

void compressor_xz_print_help(void)
{
	size_t i;

	fputs(
"Available options for xz compressor:\n"
"\n"
"    dictsize=<value>  Dictionary size. Either a value in bytes or a\n"
"                      percentage of the block size. Defaults to 100%.\n"
"                      The suffix '%' indicates a percentage. 'K' and 'M'\n"
"                      can also be used for kibi and mebi bytes\n"
"                      respecitively.\n"
"\n"
"In additon to the options, one or more bcj filters can be specified.\n"
"If multiple filters are provided, the one yielding the best compression\n"
"ratio will be used.\n"
"\n"
"The following filters are available:\n",
	stdout);

	for (i = 0; i < XZ_NUM_FILTERS; ++i)
		printf("\t%s\n", xz_filters[i].name);
}
