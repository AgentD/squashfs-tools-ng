/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream_compressor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

static int comp_append(ostream_t *strm, const void *data, size_t size)
{
	ostream_comp_t *comp = (ostream_comp_t *)strm;
	size_t diff;

	while (size > 0) {
		if (comp->inbuf_used >= BUFSZ) {
			if (comp->flush_inbuf(comp, false))
				return -1;
		}

		diff = BUFSZ - comp->inbuf_used;

		if (diff > size)
			diff = size;

		memcpy(comp->inbuf + comp->inbuf_used, data, diff);

		comp->inbuf_used += diff;
		data = (const char *)data + diff;
		size -= diff;
	}

	return 0;
}

static int comp_flush(ostream_t *strm)
{
	ostream_comp_t *comp = (ostream_comp_t *)strm;

	if (comp->inbuf_used > 0) {
		if (comp->flush_inbuf(comp, true))
			return -1;
	}

	return comp->wrapped->flush(comp->wrapped);
}

static const char *comp_get_filename(ostream_t *strm)
{
	ostream_comp_t *comp = (ostream_comp_t *)strm;

	return comp->wrapped->get_filename(comp->wrapped);
}

static void comp_destroy(sqfs_object_t *obj)
{
	ostream_comp_t *comp = (ostream_comp_t *)obj;

	comp->cleanup(comp);
	sqfs_destroy(comp->wrapped);
	free(comp);
}

ostream_t *ostream_compressor_create(ostream_t *strm, int comp_id)
{
	ostream_comp_t *comp = NULL;
	sqfs_object_t *obj;
	ostream_t *base;

	switch (comp_id) {
	case FSTREAM_COMPRESSOR_GZIP:
#ifdef WITH_GZIP
		comp = ostream_gzip_create(strm->get_filename(strm));
#endif
		break;
	case FSTREAM_COMPRESSOR_XZ:
#ifdef WITH_XZ
		comp = ostream_xz_create(strm->get_filename(strm));
#endif
		break;
	case FSTREAM_COMPRESSOR_ZSTD:
#if defined(WITH_ZSTD) && defined(HAVE_ZSTD_STREAM)
		comp = ostream_zstd_create(strm->get_filename(strm));
#endif
		break;
	case FSTREAM_COMPRESSOR_BZIP2:
#ifdef WITH_BZIP2
		comp = ostream_bzip2_create(strm->get_filename(strm));
#endif
		break;
	default:
		break;
	}

	if (comp == NULL)
		return NULL;

	comp->wrapped = strm;
	comp->inbuf_used = 0;

	base = (ostream_t *)comp;
	base->append = comp_append;
	base->flush = comp_flush;
	base->get_filename = comp_get_filename;

	obj = (sqfs_object_t *)comp;
	obj->destroy = comp_destroy;
	return base;
}
