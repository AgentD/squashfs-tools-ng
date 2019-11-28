/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * compressor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include <string.h>
#include <stdlib.h>

#include "internal.h"

typedef sqfs_compressor_t *(*compressor_fun_t)
	(const sqfs_compressor_config_t *cfg);

static compressor_fun_t compressors[SQFS_COMP_MAX + 1] = {
#ifdef WITH_GZIP
	[SQFS_COMP_GZIP] = gzip_compressor_create,
#endif
#ifdef WITH_XZ
	[SQFS_COMP_XZ] = xz_compressor_create,
	[SQFS_COMP_LZMA] = lzma_compressor_create,
#endif
#ifdef WITH_LZ4
	[SQFS_COMP_LZ4] = lz4_compressor_create,
#endif
#ifdef WITH_ZSTD
	[SQFS_COMP_ZSTD] = zstd_compressor_create,
#endif
};

static const char *names[] = {
	[SQFS_COMP_GZIP] = "gzip",
	[SQFS_COMP_LZMA] = "lzma",
	[SQFS_COMP_LZO] = "lzo",
	[SQFS_COMP_XZ] = "xz",
	[SQFS_COMP_LZ4] = "lz4",
	[SQFS_COMP_ZSTD] = "zstd",
};

int sqfs_generic_write_options(sqfs_file_t *file, const void *data, size_t size)
{
	sqfs_u8 buffer[size + 2];
	int ret;

	*((sqfs_u16 *)buffer) = htole16(0x8000 | size);
	memcpy(buffer + 2, data, size);

	ret = file->write_at(file, sizeof(sqfs_super_t),
			     buffer, sizeof(buffer));
	if (ret)
		return ret;

	return sizeof(buffer);
}

int sqfs_generic_read_options(sqfs_file_t *file, void *data, size_t size)
{
	sqfs_u8 buffer[size + 2];
	int ret;

	ret = file->read_at(file, sizeof(sqfs_super_t),
			    buffer, sizeof(buffer));
	if (ret)
		return ret;

	if (le16toh(*((sqfs_u16 *)buffer)) != (0x8000 | size))
		return SQFS_ERROR_CORRUPTED;

	memcpy(data, buffer + 2, size);
	return 0;
}

bool sqfs_compressor_exists(E_SQFS_COMPRESSOR id)
{
	if (id < SQFS_COMP_MIN || id > SQFS_COMP_MAX)
		return false;

	return (compressors[id] != NULL);
}

sqfs_compressor_t *sqfs_compressor_create(const sqfs_compressor_config_t *cfg)
{
	sqfs_u8 padd0[sizeof(cfg->opt)];
	int ret;

	if (cfg == NULL || cfg->id < SQFS_COMP_MIN || cfg->id > SQFS_COMP_MAX)
		return NULL;

	if (compressors[cfg->id] == NULL)
		return NULL;

	memset(padd0, 0, sizeof(padd0));

	switch (cfg->id) {
	case SQFS_COMP_XZ:
		ret = memcmp(cfg->opt.xz.padd0, padd0,
			     sizeof(cfg->opt.xz.padd0));
		break;
	case SQFS_COMP_LZO:
		ret = memcmp(cfg->opt.lzo.padd0, padd0,
			     sizeof(cfg->opt.lzo.padd0));
		break;
	case SQFS_COMP_ZSTD:
		ret = memcmp(cfg->opt.zstd.padd0, padd0,
			     sizeof(cfg->opt.zstd.padd0));
		break;
	case SQFS_COMP_GZIP:
		ret = memcmp(cfg->opt.gzip.padd0, padd0,
			     sizeof(cfg->opt.gzip.padd0));
		break;
	default:
		ret = memcmp(cfg->opt.padd0, padd0, sizeof(cfg->opt.padd0));
		break;
	}

	if (ret != 0)
		return NULL;

	return compressors[cfg->id](cfg);
}

const char *sqfs_compressor_name_from_id(E_SQFS_COMPRESSOR id)
{
	if (id < 0 || (size_t)id >= sizeof(names) / sizeof(names[0]))
		return NULL;

	return names[id];
}

int sqfs_compressor_id_from_name(const char *name)
{
	size_t i;

	for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (names[i] != NULL && strcmp(names[i], name) == 0)
			return i;
	}

	return SQFS_ERROR_UNSUPPORTED;
}

int sqfs_compressor_config_init(sqfs_compressor_config_t *cfg,
				E_SQFS_COMPRESSOR id,
				size_t block_size, sqfs_u16 flags)
{
	memset(cfg, 0, sizeof(*cfg));

	cfg->id = id;
	cfg->flags = flags;
	cfg->block_size = block_size;

	switch (id) {
	case SQFS_COMP_GZIP:
		cfg->opt.gzip.level = SQFS_GZIP_DEFAULT_LEVEL;
		cfg->opt.gzip.window_size = SQFS_GZIP_DEFAULT_WINDOW;
		break;
	case SQFS_COMP_LZO:
		cfg->opt.lzo.algorithm = SQFS_LZO_DEFAULT_ALG;
		cfg->opt.lzo.level = SQFS_LZO_DEFAULT_LEVEL;
		break;
	case SQFS_COMP_ZSTD:
		cfg->opt.zstd.level = SQFS_ZSTD_DEFAULT_LEVEL;
		break;
	case SQFS_COMP_XZ:
		cfg->opt.xz.dict_size = block_size;
		break;
	default:
		break;
	}

	return 0;
}
