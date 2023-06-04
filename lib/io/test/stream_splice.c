/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * stream_splice.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "io/istream.h"
#include "io/ostream.h"
#include "io/mem.h"
#include "util/test.h"

static const sqfs_u64 end0 = 449;	/* region 1: filled with 'A' */
static const sqfs_u64 end1 = 521;	/* region 2: filled with 'B' */
static const sqfs_u64 end2 = 941;	/* region 3: filled with 'C' */

static sqfs_u8 rd_buffer[941];

static sqfs_u8 byte_at_offset(sqfs_u64 off)
{
	if (off < end0)
		return 'A';
	if (off < end1)
		return 'B';
	return 'C';
}

static void init_rd_buffer(void)
{
	for (size_t i = 0; i < end2; ++i)
		rd_buffer[i] = byte_at_offset(i);
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

int main(int argc, char **argv)
{
	sqfs_u64 total = 0;
	istream_t *in;
	sqfs_s32 ret;
	(void)argc; (void)argv;

	init_rd_buffer();
	in = istream_memory_create("memory_in", 109,
				   rd_buffer, sizeof(rd_buffer));
	TEST_NOT_NULL(in);

	for (;;) {
		ret = istream_splice(in, &out, 211);
		TEST_ASSERT(ret >= 0);

		if (ret == 0)
			break;

		total += ret;
		TEST_ASSERT(total <= end2);
		TEST_ASSERT(out_offset <= end2);
		TEST_EQUAL_UI(total, out_offset);
	}

	TEST_ASSERT(in->eof);
	TEST_ASSERT(in->buffer_used == 0);
	TEST_EQUAL_UI(total, end2);
	TEST_EQUAL_UI(out_offset, end2);
	sqfs_drop(in);
	return EXIT_SUCCESS;
}
