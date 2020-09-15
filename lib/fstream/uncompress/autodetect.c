/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * autodetect.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

static const struct {
	int id;
	const sqfs_u8 *value;
	size_t len;
} magic[] = {
	{ FSTREAM_COMPRESSOR_GZIP, (const sqfs_u8 *)"\x1F\x8B\x08", 3 },
	{ FSTREAM_COMPRESSOR_XZ, (const sqfs_u8 *)("\xFD" "7zXZ"), 6 },
	{ FSTREAM_COMPRESSOR_ZSTD, (const sqfs_u8 *)"\x28\xB5\x2F\xFD", 4 },
	{ FSTREAM_COMPRESSOR_BZIP2, (const sqfs_u8 *)"BZh", 3 },
};

int istream_detect_compressor(istream_t *strm,
			      int (*probe)(const sqfs_u8 *data, size_t size))
{
	size_t i;
	int ret;

	ret = istream_precache(strm);
	if (ret != 0)
		return ret;

	if (probe != NULL) {
		ret = probe(strm->buffer + strm->buffer_offset,
			    strm->buffer_used - strm->buffer_offset);
		if (ret < 0)
			return ret;

		/* XXX: this means the data is uncompressed. We do this check
		   first since it might be perfectly OK for the uncompressed
		   data to contain a magic number from the table. */
		if (ret > 0)
			return 0;
	}

	for (i = 0; i < sizeof(magic) / sizeof(magic[0]); ++i) {
		if ((strm->buffer_used - strm->buffer_offset) < magic[i].len)
			continue;

		ret = memcmp(strm->buffer + strm->buffer_offset,
			     magic[i].value, magic[i].len);

		if (ret == 0)
			return magic[i].id;
	}

	return 0;
}
