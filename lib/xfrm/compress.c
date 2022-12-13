/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compress.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "xfrm/compress.h"
#include "config.h"

#include <string.h>

static const struct {
	int id;
	const char *name;
	const sqfs_u8 *magic;
	size_t count;
	xfrm_stream_t *(*mk_comp_stream)(const compressor_config_t *);
	xfrm_stream_t *(*mk_decomp_stream)(void);
} compressors[] = {
#ifdef WITH_GZIP
	{ XFRM_COMPRESSOR_GZIP, "gzip", (const sqfs_u8 *)"\x1F\x8B\x08", 3,
	  compressor_stream_gzip_create, decompressor_stream_gzip_create },
#endif
#ifdef WITH_XZ
	{ XFRM_COMPRESSOR_XZ, "xz", (const sqfs_u8 *)("\xFD" "7zXZ"), 6,
	  compressor_stream_xz_create, decompressor_stream_xz_create },
#endif
#if defined(WITH_ZSTD) && defined(HAVE_ZSTD_STREAM)
	{ XFRM_COMPRESSOR_ZSTD, "zstd",
	  (const sqfs_u8 *)"\x28\xB5\x2F\xFD", 4,
	  compressor_stream_zstd_create, decompressor_stream_zstd_create },
#endif
#ifdef WITH_BZIP2
	{ XFRM_COMPRESSOR_BZIP2, "bzip2", (const sqfs_u8 *)"BZh", 3,
	  compressor_stream_bzip2_create, decompressor_stream_bzip2_create },
#endif
};

int xfrm_compressor_id_from_name(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(compressors) / sizeof(compressors[0]); ++i) {
		if (strcmp(name, compressors[i].name) == 0)
			return compressors[i].id;
	}

	return -1;
}

int xfrm_compressor_id_from_magic(const void *data, size_t count)
{
	size_t i;
	int ret;

	for (i = 0; i < sizeof(compressors) / sizeof(compressors[0]); ++i) {
		if (compressors[i].count > count)
			continue;

		ret = memcmp(compressors[i].magic, data, compressors[i].count);
		if (ret == 0)
			return compressors[i].id;
	}

	return -1;
}

const char *xfrm_compressor_name_from_id(int id)
{
	size_t i;

	for (i = 0; i < sizeof(compressors) / sizeof(compressors[0]); ++i) {
		if (compressors[i].id == id)
			return compressors[i].name;
	}

	return NULL;
}

xfrm_stream_t *compressor_stream_create(int id, const compressor_config_t *cfg)
{
	size_t i;

	for (i = 0; i < sizeof(compressors) / sizeof(compressors[0]); ++i) {
		if (compressors[i].id == id)
			return compressors[i].mk_comp_stream(cfg);
	}

	return NULL;
}

xfrm_stream_t *decompressor_stream_create(int id)
{
	size_t i;

	for (i = 0; i < sizeof(compressors) / sizeof(compressors[0]); ++i) {
		if (compressors[i].id == id)
			return compressors[i].mk_decomp_stream();
	}

	return NULL;
}
