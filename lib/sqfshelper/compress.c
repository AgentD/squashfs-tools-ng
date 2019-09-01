/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compress.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "highlevel.h"

E_SQFS_COMPRESSOR compressor_get_default(void)
{
	if (compressor_exists(SQFS_COMP_XZ))
		return SQFS_COMP_XZ;

	if (compressor_exists(SQFS_COMP_ZSTD))
		return SQFS_COMP_ZSTD;

	return SQFS_COMP_GZIP;
}

void compressor_print_available(void)
{
	int i;

	fputs("Available compressors:\n", stdout);

	for (i = SQFS_COMP_MIN; i <= SQFS_COMP_MAX; ++i) {
		if (compressor_exists(i))
			printf("\t%s\n", compressor_name_from_id(i));
	}

	printf("\nDefault compressor: %s\n",
	       compressor_name_from_id(compressor_get_default()));
}
