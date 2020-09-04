/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"


static int append_sparse_fallback(ostream_t *strm, size_t size)
{
	char buffer[512];
	size_t diff;

	memset(buffer, 0, sizeof(buffer));

	while (size > 0) {
		diff = size < sizeof(buffer) ? size : sizeof(buffer);

		if (strm->append(strm, buffer, diff))
			return -1;

		size -= diff;
	}

	return 0;
}


int ostream_append(ostream_t *strm, const void *data, size_t size)
{
	return strm->append(strm, data, size);
}

int ostream_append_sparse(ostream_t *strm, size_t size)
{
	if (strm->append_sparse == NULL)
		return append_sparse_fallback(strm, size);

	return strm->append_sparse(strm, size);
}

int ostream_flush(ostream_t *strm)
{
	return strm->flush(strm);
}

const char *ostream_get_filename(ostream_t *strm)
{
	return strm->get_filename(strm);
}

sqfs_s32 ostream_append_from_istream(ostream_t *out, istream_t *in,
				     sqfs_u32 size)
{
	sqfs_s32 total = 0;
	size_t diff;

	if (size > 0x7FFFFFFF)
		size = 0x7FFFFFFF;

	while (size > 0) {
		if (in->buffer_offset >= in->buffer_used) {
			if (istream_precache(in))
				return -1;

			if (in->buffer_used == 0)
				break;
		}

		diff = in->buffer_used - in->buffer_offset;
		if (diff > size)
			diff = size;

		if (out->append(out, in->buffer + in->buffer_offset, diff))
			return -1;

		in->buffer_offset += diff;
		size -= diff;
		total += diff;
	}

	return total;
}
