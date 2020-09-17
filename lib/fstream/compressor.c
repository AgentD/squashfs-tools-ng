/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compressor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

int fstream_compressor_id_from_name(const char *name)
{
	if (strcmp(name, "gzip") == 0)
		return FSTREAM_COMPRESSOR_GZIP;

	if (strcmp(name, "xz") == 0)
		return FSTREAM_COMPRESSOR_XZ;

	if (strcmp(name, "zstd") == 0)
		return FSTREAM_COMPRESSOR_ZSTD;

	if (strcmp(name, "bzip2") == 0)
		return FSTREAM_COMPRESSOR_BZIP2;

	return -1;
}

const char *fstream_compressor_name_from_id(int id)
{
	if (id == FSTREAM_COMPRESSOR_GZIP)
		return "gzip";

	if (id == FSTREAM_COMPRESSOR_XZ)
		return "xz";

	if (id == FSTREAM_COMPRESSOR_ZSTD)
		return "zstd";

	if (id == FSTREAM_COMPRESSOR_BZIP2)
		return "bzip2";

	return NULL;
}

bool fstream_compressor_exists(int id)
{
	switch (id) {
#ifdef WITH_GZIP
	case FSTREAM_COMPRESSOR_GZIP:
		return true;
#endif
#ifdef WITH_XZ
	case FSTREAM_COMPRESSOR_XZ:
		return true;
#endif
#if defined(WITH_ZSTD) && defined(HAVE_ZSTD_STREAM)
	case FSTREAM_COMPRESSOR_ZSTD:
		return true;
#endif
#ifdef WITH_BZIP2
	case FSTREAM_COMPRESSOR_BZIP2:
		return true;
#endif
	default:
		break;
	}

	return false;
}
