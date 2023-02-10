/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream_skip.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "io/istream.h"
#include "util/test.h"

static const sqfs_u64 end0 = 449;	/* region 1: filled with 'A' */
static const sqfs_u64 end1 = 521;	/* region 2: filled with 'B' */
static const sqfs_u64 end2 = 941;	/* region 3: filled with 'C' */

static sqfs_u8 buffer[103];		/* sliding window into the file */
static sqfs_u64 offset = 0;		/* byte offset into the "file" */

static int dummy_precache(istream_t *strm);
static const char *dummy_get_filename(istream_t *strm);

static istream_t dummy = {
	{
		1,
		NULL,
		NULL,
	},
	0,
	0,
	false,
	buffer,
	dummy_precache,
	dummy_get_filename,
};

static int dummy_precache(istream_t *strm)
{
	sqfs_u8 x;

	TEST_ASSERT(strm == &dummy);

	while (strm->buffer_used < sizeof(buffer)) {
		if (offset < end0) {
			x = 'A';
		} else if (offset < end1) {
			x = 'B';
		} else if (offset < end2) {
			x = 'C';
		} else {
			strm->eof = true;
			break;
		}

		strm->buffer[strm->buffer_used++] = x;
		++offset;
	}

	return 0;
}

static const char *dummy_get_filename(istream_t *strm)
{
	TEST_ASSERT(strm == &dummy);
	return "dummy file";
}

int main(int argc, char **argv)
{
	sqfs_u8 read_buffer[61];
	sqfs_u64 read_off = 0;
	const char *name;
	(void)argc; (void)argv;

	name = istream_get_filename(&dummy);
	TEST_NOT_NULL(name);
	TEST_STR_EQUAL(name, "dummy file");

	/* region 1 */
	while (read_off < end0) {
		size_t read_diff = end0 - read_off;

		if (read_diff > sizeof(read_buffer))
			read_diff = sizeof(read_buffer);

		int ret = istream_read(&dummy, read_buffer, read_diff);
		TEST_ASSERT(ret > 0);
		TEST_ASSERT((size_t)ret <= read_diff);

		for (int i = 0; i < ret; ++i) {
			TEST_EQUAL_UI(read_buffer[i], 'A');
		}

		read_off += ret;
	}

	/* region 2 */
	{
		int ret = istream_skip(&dummy, end2 - end1);
		TEST_EQUAL_I(ret, 0);
		read_off += (end2 - end1);
	}

	/* region 3 */
	for (;;) {
		size_t read_diff = sizeof(read_buffer);

		int ret = istream_read(&dummy, read_buffer, read_diff);
		TEST_ASSERT(ret >= 0);
		TEST_ASSERT((size_t)ret <= read_diff);

		if (ret == 0) {
			TEST_EQUAL_UI(read_off, end2);
			break;
		}

		for (int i = 0; i < ret; ++i) {
			TEST_EQUAL_UI(read_buffer[i], 'C');
		}

		read_off += ret;
		TEST_ASSERT(read_off <= end2);
	}

	return EXIT_SUCCESS;
}
