/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream_compressor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

static const char *comp_get_filename(istream_t *strm)
{
	istream_comp_t *comp = (istream_comp_t *)strm;

	return comp->wrapped->get_filename(comp->wrapped);
}

static void comp_destroy(sqfs_object_t *obj)
{
	istream_comp_t *comp = (istream_comp_t *)obj;

	comp->cleanup(comp);
	sqfs_destroy(comp->wrapped);
	free(comp);
}

istream_t *istream_compressor_create(istream_t *strm, int comp_id)
{
	istream_comp_t *comp = NULL;
	sqfs_object_t *obj;
	istream_t *base;

	switch (comp_id) {
	case FSTREAM_COMPRESSOR_GZIP:
#ifdef WITH_GZIP
		comp = istream_gzip_create(strm->get_filename(strm));
#endif
		break;
	case FSTREAM_COMPRESSOR_XZ:
#ifdef WITH_XZ
		comp = istream_xz_create(strm->get_filename(strm));
#endif
		break;
	case FSTREAM_COMPRESSOR_ZSTD:
#if defined(WITH_ZSTD) && defined(HAVE_ZSTD_STREAM)
		comp = istream_zstd_create(strm->get_filename(strm));
#endif
		break;
	case FSTREAM_COMPRESSOR_BZIP2:
#ifdef WITH_BZIP2
		comp = istream_bzip2_create(strm->get_filename(strm));
#endif
		break;
	default:
		break;
	}

	if (comp == NULL)
		return NULL;

	comp->wrapped = strm;

	base = (istream_t *)comp;
	base->get_filename = comp_get_filename;
	base->buffer = comp->uncompressed;
	base->eof = false;

	obj = (sqfs_object_t *)comp;
	obj->destroy = comp_destroy;
	return base;
}
