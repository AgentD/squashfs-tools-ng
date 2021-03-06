/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * table.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"
#include "../test.h"

#include "sqfs/compressor.h"
#include "sqfs/error.h"
#include "sqfs/table.h"
#include "sqfs/io.h"

static sqfs_u8 file_data[32768];
static size_t file_used = 0;

static int dummy_read_at(sqfs_file_t *file, sqfs_u64 offset,
			 void *buffer, size_t size)
{
	(void)file;

	if (offset >= sizeof(file_data))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (size > (sizeof(file_data) - offset))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	memset(buffer, 0, size);

	if (offset < file_used) {
		if (size > (file_used - offset))
			size = file_used - offset;

		memcpy(buffer, file_data + offset, size);
	}
	return 0;
}

static int dummy_write_at(sqfs_file_t *file, sqfs_u64 offset,
			  const void *buffer, size_t size)
{
	(void)file;

	if (offset >= sizeof(file_data))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (size > (sizeof(file_data) - offset))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	if (offset > file_used)
		memset(file_data + file_used, 0, offset - file_used);

	if ((offset + size) > file_used)
		file_used = offset + size;

	memcpy(file_data + offset, buffer, size);
	return 0;
}

static sqfs_u64 dummy_get_size(const sqfs_file_t *file)
{
	(void)file;
	return file_used;
}

static sqfs_s32 dummy_compress(sqfs_compressor_t *cmp, const sqfs_u8 *in,
			       sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	(void)cmp;
	memcpy(out, in, outsize < size ? outsize : size);
	return 0;
}

static sqfs_s32 dummy_uncompress(sqfs_compressor_t *cmp, const sqfs_u8 *in,
				 sqfs_u32 size, sqfs_u8 *out, sqfs_u32 outsize)
{
	(void)cmp;
	if (outsize < size)
		return 0;
	memcpy(out, in, size);
	return size;
}

static sqfs_file_t dummy_file = {
	{ NULL, NULL },
	dummy_read_at,
	dummy_write_at,
	dummy_get_size,
	NULL,
};

static sqfs_compressor_t dummy_compressor = {
	{ NULL, NULL },
	NULL,
	NULL,
	NULL,
	dummy_compress,
};

static sqfs_compressor_t dummy_uncompressor = {
	{ NULL, NULL },
	NULL,
	NULL,
	NULL,
	dummy_uncompress,
};

/*****************************************************************************/

static sqfs_u64 table[4000];

int main(void)
{
	sqfs_u64 start, value, locations[4], *copy;
	sqfs_u16 hdr;
	size_t i;
	int ret;

	/* fill the table with data */
	for (i = 0; i < sizeof(table) / sizeof(table[0]); ++i)
		table[i] = i;

	/* serialize the table */
	ret = sqfs_write_table(&dummy_file, &dummy_compressor,
			       table, sizeof(table), &start);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(file_used, (3 * (8192 + 2) + (7424 + 2) + 4 * sizeof(sqfs_u64)));
	TEST_EQUAL_UI(start, (3 * (8192 + 2) + (7424 + 2)));

	/* check the location list */
	memcpy(locations, file_data + start, sizeof(locations));
	for (i = 0; i < 4; ++i)
		locations[i] = le64toh(locations[i]);

	TEST_EQUAL_UI(locations[0], 0);
	TEST_EQUAL_UI(locations[1], (1 * (8192 + 2)));
	TEST_EQUAL_UI(locations[2], (2 * (8192 + 2)));
	TEST_EQUAL_UI(locations[3], (3 * (8192 + 2)));

	/* check the individual blocks */
	memcpy(&hdr, file_data + locations[0], sizeof(hdr));
	hdr = le16toh(hdr);
	TEST_EQUAL_UI(hdr, (0x8000 | 8192));

	for (i = 0; i < 8192; i += sizeof(sqfs_u64)) {
		memcpy(&value, (file_data + locations[0] + 2) + i,
		       sizeof(value));

		TEST_EQUAL_UI(value, i / sizeof(sqfs_u64));
	}

	memcpy(&hdr, file_data + locations[1], sizeof(hdr));
	hdr = le16toh(hdr);
	TEST_EQUAL_UI(hdr, (0x8000 | 8192));

	for (i = 0; i < 8192; i += sizeof(sqfs_u64)) {
		memcpy(&value, (file_data + locations[1] + 2) + i,
		       sizeof(value));

		TEST_EQUAL_UI(value, (1024 + i / sizeof(sqfs_u64)));
	}

	memcpy(&hdr, file_data + locations[2], sizeof(hdr));
	hdr = le16toh(hdr);
	TEST_EQUAL_UI(hdr, (0x8000 | 8192));

	for (i = 0; i < 8192; i += sizeof(sqfs_u64)) {
		memcpy(&value, (file_data + locations[2] + 2) + i,
		       sizeof(value));

		TEST_EQUAL_UI(value, (2048 + i / sizeof(sqfs_u64)));
	}

	memcpy(&hdr, file_data + locations[3], sizeof(hdr));
	hdr = le16toh(hdr);
	TEST_EQUAL_UI(hdr, (0x8000 | 7424));

	for (i = 0; i < 7424; i += sizeof(sqfs_u64)) {
		memcpy(&value, (file_data + locations[3] + 2) + i,
		       sizeof(value));

		TEST_EQUAL_UI(value, (3072 + i / sizeof(sqfs_u64)));
	}

	/* read the table back */
	ret = sqfs_read_table(&dummy_file, &dummy_uncompressor,
			      sizeof(table), start, 0, start, (void **)&copy);
	TEST_EQUAL_I(ret, 0);

	ret = memcmp(copy, table, sizeof(table));
	TEST_EQUAL_I(ret, 0);

	free(copy);
	return EXIT_SUCCESS;
}
