/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"


sqfs_s32 istream_read(istream_t *strm, void *data, size_t size)
{
	sqfs_s32 total = 0;

	if (size > 0x7FFFFFFF)
		size = 0x7FFFFFFF;

	while (size > 0) {
		const sqfs_u8 *ptr;
		size_t diff;
		int ret;

		ret = istream_get_buffered_data(strm, &ptr, &diff, size);
		if (ret > 0)
			break;
		if (ret < 0)
			return ret;

		if (diff > size)
			diff = size;

		memcpy(data, ptr, diff);
		istream_advance_buffer(strm, diff);
		data = (char *)data + diff;
		size -= diff;
		total += diff;
	}

	return total;
}

int istream_skip(istream_t *strm, sqfs_u64 size)
{
	while (size > 0) {
		const sqfs_u8 *ptr;
		size_t diff;
		int ret;

		ret = istream_get_buffered_data(strm, &ptr, &diff, size);
		if (ret < 0)
			return ret;
		if (ret > 0) {
			fprintf(stderr, "%s: unexpected end-of-file\n",
				strm->get_filename(strm));
			return -1;
		}

		if ((sqfs_u64)diff > size)
			diff = size;

		size -= diff;
		istream_advance_buffer(strm, diff);
	}

	return 0;
}

sqfs_s32 istream_splice(istream_t *in, ostream_t *out, sqfs_u32 size)
{
	sqfs_s32 total = 0;

	if (size > 0x7FFFFFFF)
		size = 0x7FFFFFFF;

	while (size > 0) {
		const sqfs_u8 *ptr;
		size_t diff;
		int ret;

		ret = istream_get_buffered_data(in, &ptr, &diff, size);
		if (ret < 0)
			return ret;
		if (ret > 0)
			break;

		if (diff > size)
			diff = size;

		if (ostream_append(out, ptr, diff))
			return -1;

		total += diff;
		size -= diff;
		istream_advance_buffer(in, diff);
	}

	return total;
}
