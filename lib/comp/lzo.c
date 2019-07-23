/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include <lzo/lzo1x.h>

#include "internal.h"


typedef enum {
	LZO_ALGORITHM_LZO1X_1 = 0,
	LZO_ALGORITHM_LZO1X_1_11 = 1,
	LZO_ALGORITHM_LZO1X_1_12 = 2,
	LZO_ALGORITHM_LZO1X_1_15 = 3,
	LZO_ALGORITHM_LZO1X_999	= 4,
} LZO_ALGORITHM;

#define LZO_DEFAULT_ALG LZO_ALGORITHM_LZO1X_999
#define LZO_DEFAULT_LEVEL 8
#define LZO_NUM_ALGS (sizeof(lzo_algs) / sizeof(lzo_algs[0]))

typedef int (*lzo_cb_t)(const lzo_bytep src, lzo_uint src_len, lzo_bytep dst,
			lzo_uintp dst_len, lzo_voidp wrkmem);

static const struct {
	const char *name;
	lzo_cb_t compress;
	size_t bufsize;
} lzo_algs[] = {
	[LZO_ALGORITHM_LZO1X_1] = {
		.name = "lzo1x_1",
		.compress = lzo1x_1_compress,
		.bufsize = LZO1X_1_MEM_COMPRESS,
	},
	[LZO_ALGORITHM_LZO1X_1_11] = {
		.name = "lzo1x_1_11",
		.compress = lzo1x_1_11_compress,
		.bufsize = LZO1X_1_11_MEM_COMPRESS,
	},
	[LZO_ALGORITHM_LZO1X_1_12] = {
		.name = "lzo1x_1_12",
		.compress = lzo1x_1_12_compress,
		.bufsize = LZO1X_1_12_MEM_COMPRESS,
	},
	[LZO_ALGORITHM_LZO1X_1_15] = {
		.name = "lzo1x_1_15",
		.compress = lzo1x_1_15_compress,
		.bufsize = LZO1X_1_15_MEM_COMPRESS,
	},
	[LZO_ALGORITHM_LZO1X_999] = {
		.name = "lzo1x_999",
		.compress = lzo1x_999_compress,
		.bufsize = LZO1X_999_MEM_COMPRESS,
	},
};

typedef struct {
	compressor_t base;
	int algorithm;
	int level;

	uint8_t buffer[];
} lzo_compressor_t;

typedef struct {
	uint32_t algorithm;
	uint32_t level;
} lzo_options_t;

static int lzo_write_options(compressor_t *base, int fd)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_options_t opt;

	if (lzo->algorithm == LZO_DEFAULT_ALG &&
	    lzo->level == LZO_DEFAULT_LEVEL) {
		return 0;
	}

	opt.algorithm = htole32(lzo->algorithm);

	if (lzo->algorithm == LZO_ALGORITHM_LZO1X_999) {
		opt.level = htole32(lzo->level);
	} else {
		opt.level = 0;
	}

	return generic_write_options(fd, &opt, sizeof(opt));
}

static int lzo_read_options(compressor_t *base, int fd)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_options_t opt;

	if (generic_read_options(fd, &opt, sizeof(opt)))
		return -1;

	lzo->algorithm = le32toh(opt.algorithm);
	lzo->level = le32toh(opt.level);

	switch(lzo->algorithm) {
	case LZO_ALGORITHM_LZO1X_1:
	case LZO_ALGORITHM_LZO1X_1_11:
	case LZO_ALGORITHM_LZO1X_1_12:
	case LZO_ALGORITHM_LZO1X_1_15:
		if (lzo->level != 0)
			goto fail_level;
		break;
	case LZO_ALGORITHM_LZO1X_999:
		if (lzo->level < 1 || lzo->level > 9)
			goto fail_level;
		break;
	default:
		fputs("Unsupported LZO variant specified.\n", stderr);
		return -1;
	}

	return 0;
fail_level:
	fputs("Unsupported LZO compression level specified.\n", stderr);
	return -1;
}

static ssize_t lzo_comp_block(compressor_t *base, const uint8_t *in,
			      size_t size, uint8_t *out, size_t outsize)
{
	lzo_compressor_t *lzo = (lzo_compressor_t *)base;
	lzo_uint len = outsize;
	int ret;

	if (lzo->algorithm == LZO_ALGORITHM_LZO1X_999 &&
	    lzo->level != LZO_DEFAULT_LEVEL) {
		ret = lzo1x_999_compress_level(in, size, out, &len,
					       lzo->buffer, NULL, 0, 0,
					       lzo->level);
	} else {
		ret = lzo_algs[lzo->algorithm].compress(in, size, out,
							&len, lzo->buffer);
	}

	if (ret != LZO_E_OK) {
		fputs("LZO compression failed.\n", stderr);
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

static int process_options(char *options, int *algorithm, int *level)
{
	enum {
		OPT_ALG = 0,
		OPT_LEVEL,
	};
	char *const token[] = {
		[OPT_ALG] = (char *)"algorithm",
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
		case OPT_ALG:
			if (value == NULL)
				goto fail_value;

			for (i = 0; i < LZO_NUM_ALGS; ++i) {
				if (strcmp(lzo_algs[i].name, value) == 0) {
					*algorithm = i;
					break;
				}
			}

			if (i == LZO_NUM_ALGS) {
				fprintf(stderr, "Unknown lzo variant '%s'.\n",
					value);
				return -1;
			}
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
			goto fail_opt;
		}
	}

	return 0;
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

compressor_t *create_lzo_compressor(bool compress, size_t block_size,
				    char *options)
{
	lzo_compressor_t *lzo;
	compressor_t *base;
	int level, alg;
	(void)block_size;

	alg = LZO_DEFAULT_ALG;
	level = LZO_DEFAULT_LEVEL;

	if (options != NULL && process_options(options, &alg, &level) != 0)
		return NULL;

	lzo = calloc(1, sizeof(*lzo) + lzo_algs[alg].bufsize);
	base = (compressor_t *)lzo;

	if (lzo == NULL) {
		perror("creating lzo compressor");
		return NULL;
	}

	lzo->algorithm = alg;
	lzo->level = level;

	base->destroy = lzo_destroy;
	base->do_block = compress ? lzo_comp_block : lzo_uncomp_block;
	base->write_options = lzo_write_options;
	base->read_options = lzo_read_options;
	return base;
}

void compressor_lzo_print_help(void)
{
	size_t i;

	fputs("Available options for lzo compressor:\n"
	      "\n"
	      "    algorithm=<name>  Specify the variant of lzo to use.\n"
	      "                      Defaults to 'lzo1x_999'.\n"
	      "    level=<value>     For lzo1x_999, the compression level.\n"
	      "                      Value from 1 to 9. Defaults to 8.\n"
	      "                      Ignored if algorithm is not lzo1x_999.\n"
	      "\n"
	      "Available algorithms:\n",
	      stdout);

	for (i = 0; i < LZO_NUM_ALGS; ++i)
		printf("\t%s\n", lzo_algs[i].name);
}
