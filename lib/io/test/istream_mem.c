/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream_mem.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "io/mem.h"

static const size_t end0 = 449;	/* region 1: filled with 'A' */
static const size_t end1 = 521;	/* region 2: filled with 'B' */
static const size_t end2 = 941;	/* region 3: filled with 'C' */

static unsigned char data[941];

static unsigned char byte_at_offset(size_t off)
{
	if (off < end0)
		return 'A';
	if (off < end1)
		return 'B';
	return 'C';
}

static void init_buffer(void)
{
	for (size_t i = 0; i < end2; ++i)
		data[i] = byte_at_offset(i);
}

int main(int argc, char **argv)
{
	size_t i, diff, size;
	bool eat_all = true;
	const sqfs_u8 *ptr;
	istream_t *in;
	int ret;
	(void)argc; (void)argv;

	init_buffer();

	in = istream_memory_create("memstream.txt", 61, data, sizeof(data));
	TEST_NOT_NULL(in);
	TEST_EQUAL_UI(((sqfs_object_t *)in)->refcount, 1);

	TEST_STR_EQUAL(istream_get_filename(in), "memstream.txt");

	for (i = 0; i < end2; i += diff) {
		ret = istream_get_buffered_data(in, &ptr, &size, 61);
		TEST_EQUAL_I(ret, 0);

		if ((end2 - i) >= 61) {
			TEST_NOT_NULL(ptr);
			TEST_EQUAL_UI(size, 61);
		} else {
			TEST_NOT_NULL(ptr);
			TEST_EQUAL_UI(size, (end2 - i));
		}

		for (size_t j = 0; j < size; ++j) {
			TEST_EQUAL_UI(ptr[j], byte_at_offset(i + j));
		}

		diff = eat_all ? size : (size / 2);
		eat_all = !eat_all;

		ret = istream_advance_buffer(in, diff);
		TEST_EQUAL_I(ret, 0);
	}

	sqfs_drop(in);
	return EXIT_SUCCESS;
}
