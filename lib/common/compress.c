/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compress.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

static int cmp_ids[] = {
	SQFS_COMP_XZ,
	SQFS_COMP_ZSTD,
	SQFS_COMP_GZIP,
	SQFS_COMP_LZ4,
	SQFS_COMP_LZO,
};

SQFS_COMPRESSOR compressor_get_default(void)
{
	sqfs_compressor_config_t cfg;
	sqfs_compressor_t *temp;
	size_t i;
	int ret;

	for (i = 0; i < sizeof(cmp_ids) / sizeof(cmp_ids[0]); ++i) {
		sqfs_compressor_config_init(&cfg, cmp_ids[i],
					    SQFS_DEFAULT_BLOCK_SIZE, 0);

		ret = sqfs_compressor_create(&cfg, &temp);

		if (ret == 0) {
			sqfs_destroy(temp);
			return cmp_ids[i];
		}
	}

#ifdef WITH_LZO
	return SQFS_COMP_LZO;
#else
	assert(0);
#endif
}

void compressor_print_available(void)
{
	sqfs_compressor_config_t cfg;
	sqfs_compressor_t *temp;
	bool have_compressor;
	int i, ret;

	fputs("Available compressors:\n", stdout);

	for (i = SQFS_COMP_MIN; i <= SQFS_COMP_MAX; ++i) {
		sqfs_compressor_config_init(&cfg, i,
					    SQFS_DEFAULT_BLOCK_SIZE, 0);

		ret = sqfs_compressor_create(&cfg, &temp);

		if (ret == 0) {
			sqfs_destroy(temp);
			have_compressor = true;
		} else {
#ifdef WITH_LZO
			if (i == SQFS_COMP_LZO)
				have_compressor = true;
#endif
		}

		if (have_compressor)
			printf("\t%s\n", sqfs_compressor_name_from_id(i));
	}

	printf("\nDefault compressor: %s\n",
	       sqfs_compressor_name_from_id(compressor_get_default()));
}
