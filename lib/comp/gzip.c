/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <zlib.h>

#include "internal.h"

#define GZIP_DEFAULT_LEVEL 9
#define GZIP_DEFAULT_WINDOW 15
#define GZIP_NUM_STRATEGIES (sizeof(strategies) / sizeof(strategies[0]))

typedef enum {
	GZIP_STRATEGY_DEFAULT = 0x01,
	GZIP_STRATEGY_FILTERED = 0x02,
	GZIP_STRATEGY_HUFFMAN = 0x04,
	GZIP_STARTEGY_RLE = 0x08,
	GZIP_STRATEGY_FIXED = 0x10,

	GZIP_ALL_STRATEGIES = 0x1F,
} GZIP_STRATEGIES;

static const struct {
	const char *name;
	int flag;
	int zlib;
} strategies[] = {
	{ "default", GZIP_STRATEGY_DEFAULT, Z_DEFAULT_STRATEGY },
	{ "filtered", GZIP_STRATEGY_FILTERED, Z_FILTERED },
	{ "huffman", GZIP_STRATEGY_HUFFMAN, Z_HUFFMAN_ONLY },
	{ "rle", GZIP_STARTEGY_RLE, Z_RLE },
	{ "fixed", GZIP_STRATEGY_FIXED, Z_FIXED },
};

typedef struct {
	uint32_t level;
	uint16_t window;
	uint16_t strategies;
} gzip_options_t;

typedef struct {
	compressor_t base;

	z_stream strm;
	bool compress;

	size_t block_size;
	gzip_options_t opt;
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
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;
	gzip_options_t opt;

	if (gzip->opt.level == GZIP_DEFAULT_LEVEL &&
	    gzip->opt.window == GZIP_DEFAULT_WINDOW &&
	    gzip->opt.strategies == 0) {
		return 0;
	}

	opt.level = htole32(gzip->opt.level);
	opt.window = htole16(gzip->opt.window);
	opt.strategies = htole16(gzip->opt.strategies);

	return generic_write_options(fd, &opt, sizeof(opt));
}

static int gzip_read_options(compressor_t *base, int fd)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;
	gzip_options_t opt;

	if (generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	gzip->opt.level = le32toh(opt.level);
	gzip->opt.window = le16toh(opt.window);
	gzip->opt.strategies = le16toh(opt.strategies);

	if (gzip->opt.level < 1 || gzip->opt.level > 9) {
		fprintf(stderr, "Invalid gzip compression level '%d'.\n",
			gzip->opt.level);
		return -1;
	}

	if (gzip->opt.window < 8 || gzip->opt.window > 15) {
		fprintf(stderr, "Invalid gzip window size '%d'.\n",
			gzip->opt.window);
		return -1;
	}

	if (gzip->opt.strategies & ~GZIP_ALL_STRATEGIES) {
		fputs("Unknown gzip strategies selected.\n", stderr);
		return -1;
	}

	return 0;
}

static int find_strategy(gzip_compressor_t *gzip, const uint8_t *in,
			 size_t size, uint8_t *out, size_t outsize)
{
	int ret, selected = Z_DEFAULT_STRATEGY;
	size_t i, length, minlength = 0;

	for (i = 0; i < GZIP_NUM_STRATEGIES; ++i) {
		if (!(strategies[i].flag & gzip->opt.strategies))
			continue;

		ret = deflateReset(&gzip->strm);
		if (ret != Z_OK) {
			fputs("resetting zlib stream failed\n",
			      stderr);
			return -1;
		}

		gzip->strm.next_in = (void *)in;
		gzip->strm.avail_in = size;
		gzip->strm.next_out = out;
		gzip->strm.avail_out = outsize;

		ret = deflateParams(&gzip->strm, gzip->opt.level,
				    strategies[i].zlib);
		if (ret != Z_OK) {
			fputs("setting deflate parameters failed\n",
			      stderr);
			return -1;
		}

		ret = deflate(&gzip->strm, Z_FINISH);

		if (ret == Z_STREAM_END) {
			length = gzip->strm.total_out;

			if (minlength == 0 || length < minlength) {
				minlength = length;
				selected = strategies[i].zlib;
			}
		} else if (ret != Z_OK && ret != Z_BUF_ERROR) {
			fputs("gzip block processing failed\n", stderr);
			return -1;
		}
	}

	return selected;
}

static ssize_t gzip_do_block(compressor_t *base, const uint8_t *in,
			     size_t size, uint8_t *out, size_t outsize)
{
	gzip_compressor_t *gzip = (gzip_compressor_t *)base;
	int ret, strategy = 0;
	size_t written;

	if (gzip->compress && gzip->opt.strategies != 0) {
		strategy = find_strategy(gzip, in, size, out, outsize);
		if (strategy < 0)
			return -1;
	}

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

	if (gzip->compress && gzip->opt.strategies != 0) {
		ret = deflateParams(&gzip->strm, gzip->opt.level, strategy);
		if (ret != Z_OK) {
			fputs("setting selcted deflate parameters failed\n",
			      stderr);
			return -1;
		}
	}

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

	if (ret != Z_OK && ret != Z_BUF_ERROR) {
		fputs("gzip block processing failed\n", stderr);
		return -1;
	}

	return 0;
}

static int process_options(char *options, int *level, int *window, int *flags)
{
	enum {
		OPT_WINDOW = 0,
		OPT_LEVEL,
	};
	char *const token[] = {
		[OPT_WINDOW] = (char *)"window",
		[OPT_LEVEL] = (char *)"level",
		NULL
	};
	char *subopts, *value;
	size_t i;
	int opt;

	subopts = options;

	while (*subopts != '\0') {
		opt = getsubopt(&subopts, token, &value);

		switch (opt) {
		case OPT_WINDOW:
			if (value == NULL)
				goto fail_value;

			for (i = 0; isdigit(value[i]); ++i)
				;

			if (i < 1 || i > 3 || value[i] != '\0')
				goto fail_window;

			*window = atoi(value);

			if (*window < 8 || *window > 15)
				goto fail_window;
			break;
		case OPT_LEVEL:
			if (value == NULL)
				goto fail_value;

			for (i = 0; isdigit(value[i]); ++i)
				;

			if (i < 1 || i > 3 || value[i] != '\0')
				goto fail_level;

			*level = atoi(value);

			if (*level < 1 || *level > 9)
				goto fail_level;
			break;
		default:
			for (i = 0; i < GZIP_NUM_STRATEGIES; ++i) {
				if (strcmp(value, strategies[i].name) == 0) {
					*flags |= strategies[i].flag;
					break;
				}
			}
			if (i == GZIP_NUM_STRATEGIES)
				goto fail_opt;
			break;
		}
	}

	return 0;
fail_window:
	fputs("Window size must be a number between 8 and 15.\n", stderr);
	return -1;
fail_level:
	fputs("Compression level must be a number between 1 and 9.\n", stderr);
	return -1;
fail_opt:
	fprintf(stderr, "Unknown option '%s'.\n", value);
	return -1;
fail_value:
	fprintf(stderr, "Missing value for '%s'.\n", token[opt]);
	return -1;
}

compressor_t *create_gzip_compressor(bool compress, size_t block_size,
				     char *options)
{
	int window = GZIP_DEFAULT_WINDOW;
	int level = GZIP_DEFAULT_LEVEL;
	gzip_compressor_t *gzip;
	compressor_t *base;
	int flags = 0;
	int ret;

	if (options != NULL) {
		if (process_options(options, &level, &window, &flags))
			return NULL;
	}

	gzip = calloc(1, sizeof(*gzip));
	base = (compressor_t *)gzip;

	if (gzip == NULL) {
		perror("creating gzip compressor");
		return NULL;
	}

	gzip->opt.level = level;
	gzip->opt.window = window;
	gzip->opt.strategies = flags;
	gzip->compress = compress;
	gzip->block_size = block_size;
	base->do_block = gzip_do_block;
	base->destroy = gzip_destroy;
	base->write_options = gzip_write_options;
	base->read_options = gzip_read_options;

	if (compress) {
		ret = deflateInit2(&gzip->strm, level, Z_DEFLATED, window, 8,
				   Z_DEFAULT_STRATEGY);
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
	size_t i;

	printf(
"Available options for gzip compressor:\n"
"\n"
"    level=<value>    Compression level. Value from 1 to 9.\n"
"                     Defaults to %d.\n"
"    window=<size>    Deflate compression window size. Value from 8 to 15.\n"
"                     Defaults to %d.\n"
"\n"
"In additon to the options, one or more strategies can be specified.\n"
"If multiple stratgies are provided, the one yielding the best compression\n"
"ratio will be used.\n"
"\n"
"The following strategies are available:\n",
	GZIP_DEFAULT_LEVEL, GZIP_DEFAULT_WINDOW);

	for (i = 0; i < GZIP_NUM_STRATEGIES; ++i)
		printf("\t%s\n", strategies[i].name);
}
