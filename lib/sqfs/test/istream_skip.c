/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream_skip.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/io.h"
#include "util/test.h"
#include "io/mem.h"

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

int main(int argc, char **argv)
{
	sqfs_u8 read_buffer[61];
	sqfs_u64 read_off = 0;
	sqfs_istream_t *dummy;
	(void)argc; (void)argv;

	init_rd_buffer();
	dummy = istream_memory_create("dummy file", 103,
				      rd_buffer, sizeof(rd_buffer));
	TEST_NOT_NULL(dummy);

	/* region 1 */
	while (read_off < end0) {
		size_t read_diff = end0 - read_off;

		if (read_diff > sizeof(read_buffer))
			read_diff = sizeof(read_buffer);

		int ret = sqfs_istream_read(dummy, read_buffer, read_diff);
		TEST_ASSERT(ret > 0);
		TEST_ASSERT((size_t)ret <= read_diff);

		for (int i = 0; i < ret; ++i) {
			TEST_EQUAL_UI(read_buffer[i], 'A');
		}

		read_off += ret;
	}

	/* region 2 */
	{
		int ret = sqfs_istream_skip(dummy, end2 - end1);
		TEST_EQUAL_I(ret, 0);
		read_off += (end2 - end1);
	}

	/* region 3 */
	for (;;) {
		size_t read_diff = sizeof(read_buffer);

		int ret = sqfs_istream_read(dummy, read_buffer, read_diff);
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

	sqfs_drop(dummy);
	return EXIT_SUCCESS;
}
