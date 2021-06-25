/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * comp_opt.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <ctype.h>

typedef struct {
	const char *name;
	sqfs_u16 flag;
} flag_t;

static const flag_t gzip_flags[] = {
	{ "default", SQFS_COMP_FLAG_GZIP_DEFAULT },
	{ "filtered", SQFS_COMP_FLAG_GZIP_FILTERED },
	{ "huffman", SQFS_COMP_FLAG_GZIP_HUFFMAN },
	{ "rle", SQFS_COMP_FLAG_GZIP_RLE },
	{ "fixed", SQFS_COMP_FLAG_GZIP_FIXED },
};

static const flag_t xz_flags[] = {
	{ "x86", SQFS_COMP_FLAG_XZ_X86 },
	{ "powerpc", SQFS_COMP_FLAG_XZ_POWERPC },
	{ "ia64", SQFS_COMP_FLAG_XZ_IA64 },
	{ "arm", SQFS_COMP_FLAG_XZ_ARM },
	{ "armthumb", SQFS_COMP_FLAG_XZ_ARMTHUMB },
	{ "sparc", SQFS_COMP_FLAG_XZ_SPARC },
	{ "extreme", SQFS_COMP_FLAG_XZ_EXTREME },
};

static const flag_t lzma_flags[] = {
	{ "extreme", SQFS_COMP_FLAG_LZMA_EXTREME },
};

static const flag_t lz4_flags[] = {
	{ "hc", SQFS_COMP_FLAG_LZ4_HC },
};

static const struct {
	const flag_t *flags;
	size_t count;
} comp_flags[SQFS_COMP_MAX + 1] = {
	[SQFS_COMP_GZIP] = { gzip_flags, sizeof(gzip_flags) / sizeof(flag_t) },
	[SQFS_COMP_XZ] = { xz_flags, sizeof(xz_flags) / sizeof(flag_t) },
	[SQFS_COMP_LZMA] = { lzma_flags, sizeof(lzma_flags) / sizeof(flag_t) },
	[SQFS_COMP_LZ4] = { lz4_flags, sizeof(lz4_flags) / sizeof(flag_t) },
};

static const char *lzo_algs[] = {
	[SQFS_LZO1X_1] = "lzo1x_1",
	[SQFS_LZO1X_1_11] = "lzo1x_1_11",
	[SQFS_LZO1X_1_12] = "lzo1x_1_12",
	[SQFS_LZO1X_1_15] = "lzo1x_1_15",
	[SQFS_LZO1X_999] = "lzo1x_999",
};

static int set_flag(sqfs_compressor_config_t *cfg, const char *name)
{
	const flag_t *flags = comp_flags[cfg->id].flags;
	size_t i, num_flags = comp_flags[cfg->id].count;

	for (i = 0; i < num_flags; ++i) {
		if (strcmp(flags[i].name, name) == 0) {
			cfg->flags |= flags[i].flag;
			return 0;
		}
	}

	return -1;
}

static int find_lzo_alg(sqfs_compressor_config_t *cfg, const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(lzo_algs) / sizeof(lzo_algs[0]); ++i) {
		if (strcmp(lzo_algs[i], name) == 0) {
			cfg->opt.lzo.algorithm = i;
			return 0;
		}
	}

	return -1;
}

enum {
	OPT_WINDOW = 0,
	OPT_LEVEL,
	OPT_ALG,
	OPT_DICT,
	OPT_LC,
	OPT_LP,
	OPT_PB,
	OPT_COUNT,
};
static char *const token[] = {
	[OPT_WINDOW] = (char *)"window",
	[OPT_LEVEL] = (char *)"level",
	[OPT_ALG] = (char *)"algorithm",
	[OPT_DICT] = (char *)"dictsize",
	[OPT_LC] = (char *)"lc",
	[OPT_LP] = (char *)"lp",
	[OPT_PB] = (char *)"pb",
	NULL
};

static int opt_available[SQFS_COMP_MAX + 1] = {
	[SQFS_COMP_GZIP] = (1 << OPT_WINDOW) | (1 << OPT_LEVEL),
	[SQFS_COMP_XZ] = (1 << OPT_LEVEL) | (1 << OPT_DICT) | (1 << OPT_LC) |
	                 (1 << OPT_LP) | (1 << OPT_PB),
	[SQFS_COMP_LZMA] = (1 << OPT_LEVEL) | (1 << OPT_DICT) | (1 << OPT_LC) |
	                   (1 << OPT_LP) | (1 << OPT_PB),
	[SQFS_COMP_ZSTD] = (1 << OPT_LEVEL),
	[SQFS_COMP_LZO] = (1 << OPT_LEVEL) | (1 << OPT_ALG),
};

static const struct {
	int min;
	int max;
} value_range[SQFS_COMP_MAX + 1][OPT_COUNT] = {
	[SQFS_COMP_GZIP] = {
		[OPT_LEVEL] = { SQFS_GZIP_MIN_LEVEL, SQFS_GZIP_MAX_LEVEL },
		[OPT_WINDOW] = { SQFS_GZIP_MIN_WINDOW, SQFS_GZIP_MAX_WINDOW },
	},
	[SQFS_COMP_XZ] = {
		[OPT_LEVEL] = { SQFS_XZ_MIN_LEVEL, SQFS_XZ_MAX_LEVEL },
		[OPT_DICT] = { SQFS_XZ_MIN_DICT_SIZE, SQFS_XZ_MAX_DICT_SIZE },
		[OPT_LC] = { SQFS_XZ_MIN_LC, SQFS_XZ_MAX_LC },
		[OPT_LP] = { SQFS_XZ_MIN_LP, SQFS_XZ_MAX_LP },
		[OPT_PB] = { SQFS_XZ_MIN_PB, SQFS_XZ_MAX_PB },
	},
	[SQFS_COMP_LZMA] = {
		[OPT_LEVEL] = { SQFS_LZMA_MIN_LEVEL, SQFS_LZMA_MAX_LEVEL },
		[OPT_DICT] = { SQFS_LZMA_MIN_DICT_SIZE,
			       SQFS_LZMA_MAX_DICT_SIZE },
		[OPT_LC] = { SQFS_LZMA_MIN_LC, SQFS_LZMA_MAX_LC },
		[OPT_LP] = { SQFS_LZMA_MIN_LP, SQFS_LZMA_MAX_LP },
		[OPT_PB] = { SQFS_LZMA_MIN_PB, SQFS_LZMA_MAX_PB },
	},
	[SQFS_COMP_ZSTD] = {
		[OPT_LEVEL] = { SQFS_ZSTD_MIN_LEVEL, SQFS_ZSTD_MAX_LEVEL },
	},
	[SQFS_COMP_LZO] = {
		[OPT_LEVEL] = { SQFS_LZO_MIN_LEVEL, SQFS_LZO_MAX_LEVEL },
	},
};

int compressor_cfg_init_options(sqfs_compressor_config_t *cfg,
				SQFS_COMPRESSOR id,
				size_t block_size, char *options)
{
	char *subopts, *value;
	int opt, ival;
	size_t szval;

	if (sqfs_compressor_config_init(cfg, id, block_size, 0))
		return -1;

	if (options == NULL)
		return 0;

	subopts = options;

	while (*subopts != '\0') {
		opt = getsubopt(&subopts, token, &value);

		if (opt < 0) {
			if (set_flag(cfg, value))
				goto fail_opt;
			continue;
		}

		if (!(opt_available[cfg->id] & (1 << opt)))
			goto fail_opt;

		if (value == NULL)
			goto fail_value;

		if (opt == OPT_ALG) {
			if (find_lzo_alg(cfg, value))
				goto fail_lzo_alg;
			continue;
		}

		if (opt == OPT_DICT) {
			if (parse_size("Parsing LZMA dictionary size",
				       &szval, value, cfg->block_size)) {
				return -1;
			}
			ival = szval;
		} else {
			ival = strtol(value, NULL, 10);
		}

		if (ival < value_range[cfg->id][opt].min)
			goto fail_range;
		if (ival > value_range[cfg->id][opt].max)
			goto fail_range;

		switch (opt) {
		case OPT_LEVEL: cfg->level = ival; break;
		case OPT_LC: cfg->opt.xz.lc = ival; break;
		case OPT_LP: cfg->opt.xz.lp = ival; break;
		case OPT_PB: cfg->opt.xz.pb = ival; break;
		case OPT_WINDOW: cfg->opt.gzip.window_size = ival; break;
		case OPT_DICT: cfg->opt.xz.dict_size = ival; break;
		default:
			break;
		}
	}

	if (cfg->id == SQFS_COMP_XZ || cfg->id == SQFS_COMP_LZMA) {
		if ((cfg->opt.xz.lp + cfg->opt.xz.lc) > 4)
			goto fail_sum_lp_lc;
	}

	return 0;
fail_sum_lp_lc:
	fputs("Sum of XZ lc + lp must not exceed 4.\n", stderr);
	return -1;
fail_lzo_alg:
	fprintf(stderr, "Unknown lzo variant '%s'.\n", value);
	return -1;
fail_range:
	fprintf(stderr, "`%s` must be a number between %d and %d.\n",
		token[opt], value_range[cfg->id][opt].min,
		value_range[cfg->id][opt].max);
	return -1;
fail_opt:
	fprintf(stderr, "Unknown compressor option '%s'.\n", value);
	return -1;
fail_value:
	fprintf(stderr, "Missing value for compressor option '%s'.\n",
		token[opt]);
	return -1;
}

typedef void (*compressor_help_fun_t)(void);

static void gzip_print_help(void)
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
	SQFS_GZIP_DEFAULT_LEVEL, SQFS_GZIP_DEFAULT_WINDOW);

	for (i = 0; i < sizeof(gzip_flags) / sizeof(gzip_flags[0]); ++i)
		printf("\t%s\n", gzip_flags[i].name);
}

static void lz4_print_help(void)
{
	fputs("Available options for lz4 compressor:\n"
	      "\n"
	      "    hc    If present, use slower but better compressing\n"
	      "          variant of lz4.\n"
	      "\n",
	      stdout);
}

static void lzo_print_help(void)
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

	for (i = 0; i < sizeof(lzo_algs) / sizeof(lzo_algs[0]); ++i)
		printf("\t%s\n", lzo_algs[i]);
}

static void xz_lzma_print_help(void)
{
	size_t i;

	printf(
"Available options for LZMA and XZ (LZMA v2) compressors:\n"
"\n"
"    dictsize=<value>  Dictionary size. Either a value in bytes or a\n"
"                      percentage of the block size. Defaults to 100%%.\n"
"                      The suffix '%%' indicates a percentage. 'K' and 'M'\n"
"                      can also be used for kibi and mebi bytes\n"
"                      respecitively.\n"
"    level=<value>     Compression level. Value from %d to %d.\n"
"                      For XZ, defaults to %d, for LZMA defaults to %d.\n"
"    lc=<value>        Number of literal context bits.\n"
"                      How many of the highest bits of the previous\n"
"                      uncompressed byte to take into account when\n"
"                      predicting the bits of the next byte.\n"
"                      Default is %d.\n"
"    lp=<value>        Number of literal position bits.\n"
"                      Affects what kind of alignment in the uncompressed\n"
"                      data is assumed when encoding bytes.\n"
"                      Default is %d.\n"
"    pb=<value>        Number of position bits.\n"
"                      This is the log2 of the assumed underlying alignment\n"
"                      of the input data, i.e. pb=0 means single byte\n"
"                      allignment, pb=1 means 16 bit, 2 means 32 bit.\n"
"                      Default is %d.\n"
"    extreme           If this flag is set, try to crunch the data extra hard\n"
"                      without increasing the decompressors memory\n"
"                      requirements."
"\n"
"If values are set, the sum of lc + lp must not exceed 4.\n"
"The maximum for pb is %d.\n"
"\n"
"In additon to the options, for the XZ compressor, one or more bcj filters\n"
"can be specified.\n"
"If multiple filters are provided, the one yielding the best compression\n"
"ratio will be used.\n"
"\n"
"The following filters are available:\n",
	SQFS_XZ_MIN_LEVEL, SQFS_XZ_MAX_LEVEL,
	SQFS_XZ_DEFAULT_LEVEL, SQFS_LZMA_DEFAULT_LEVEL,
	SQFS_XZ_DEFAULT_LC, SQFS_XZ_DEFAULT_LP, SQFS_XZ_DEFAULT_PB,
	SQFS_XZ_MAX_PB);

	for (i = 0; i < sizeof(xz_flags) / sizeof(xz_flags[0]); ++i)
		printf("\t%s\n", xz_flags[i].name);
}

static void zstd_print_help(void)
{
	printf("Available options for zstd compressor:\n"
	       "\n"
	       "    level=<value>    Set compression level. Defaults to %d.\n"
	       "                     Maximum is %d.\n"
	       "\n",
	       SQFS_ZSTD_DEFAULT_LEVEL, SQFS_ZSTD_MAX_LEVEL);
}

static const compressor_help_fun_t helpfuns[SQFS_COMP_MAX + 1] = {
	[SQFS_COMP_GZIP] = gzip_print_help,
	[SQFS_COMP_XZ] = xz_lzma_print_help,
	[SQFS_COMP_LZMA] = xz_lzma_print_help,
	[SQFS_COMP_LZO] = lzo_print_help,
	[SQFS_COMP_LZ4] = lz4_print_help,
	[SQFS_COMP_ZSTD] = zstd_print_help,
};

void compressor_print_help(SQFS_COMPRESSOR id)
{
	if (id < SQFS_COMP_MIN || id > SQFS_COMP_MAX)
		return;

	if (helpfuns[id] == NULL)
		return;

	helpfuns[id]();
}
