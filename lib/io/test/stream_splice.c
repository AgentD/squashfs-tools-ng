/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * stream_splice.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "io/istream.h"
#include "io/ostream.h"
#include "util/test.h"

static const sqfs_u64 end0 = 449;	/* region 1: filled with 'A' */
static const sqfs_u64 end1 = 521;	/* region 2: filled with 'B' */
static const sqfs_u64 end2 = 941;	/* region 3: filled with 'C' */

static sqfs_u8 byte_at_offset(sqfs_u64 off)
{
	if (off < end0)
		return 'A';
	if (off < end1)
		return 'B';
	return 'C';
}

/*****************************************************************************/

static int out_append(ostream_t *strm, const void *data, size_t size);

static sqfs_u64 out_offset = 0;

static ostream_t out = {
	{ 1, NULL, NULL },
	out_append,
	NULL,
	NULL,
	NULL,
};

static int out_append(ostream_t *strm, const void *data, size_t size)
{
	const sqfs_u8 *ptr = data;

	TEST_ASSERT(strm == &out);
	TEST_ASSERT(size > 0);

	while (size--) {
		sqfs_u8 x = *(ptr++);
		sqfs_u8 y = byte_at_offset(out_offset++);

		TEST_EQUAL_UI(x, y);
		TEST_ASSERT(out_offset <= end2);
	}

	return 0;
}

/*****************************************************************************/

static int in_precache(istream_t *strm);

static sqfs_u8 in_buffer[109];
static sqfs_u64 in_offset = 0;

static istream_t in = {
	{ 1, NULL, NULL },
	0,
	0,
	false,
	in_buffer,
	in_precache,
	NULL,
};

static int in_precache(istream_t *strm)
{
	TEST_ASSERT(strm == &in);

	while (strm->buffer_used < sizeof(in_buffer)) {
		if (in_offset == end2) {
			strm->eof = true;
			break;
		}

		in_buffer[strm->buffer_used++] = byte_at_offset(in_offset++);
	}

	return 0;
}

/*****************************************************************************/

int main(int argc, char **argv)
{
	sqfs_u64 total = 0;
	sqfs_s32 ret;
	(void)argc; (void)argv;

	for (;;) {
		ret = ostream_append_from_istream(&out, &in, 211);
		TEST_ASSERT(ret >= 0);

		if (ret == 0)
			break;

		total += ret;
		TEST_ASSERT(total <= end2);
		TEST_ASSERT(out_offset <= end2);
		TEST_ASSERT(in_offset <= end2);
		TEST_ASSERT(in_offset >= out_offset);
		TEST_EQUAL_UI(total, out_offset);
	}

	TEST_EQUAL_UI(total, end2);
	TEST_EQUAL_UI(out_offset, end2);
	TEST_EQUAL_UI(in_offset, end2);
	return EXIT_SUCCESS;
}
