/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * parse_int.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/parse.h"
#include "util/test.h"
#include "sqfs/error.h"

int main(int argc, char **argv)
{
	sqfs_s64 s_out;
	sqfs_u64 out;
	size_t diff;
	int ret;
	(void)argc; (void)argv;

	/* must begin with a digit */
	ret = parse_uint("a1234", -1, &diff, 0, 0, &out);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);

	/* can end with a non-digit... */
	ret = parse_uint("1234a", -1, &diff, 0, 0, &out);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(out, 1234);
	TEST_EQUAL_UI(diff, 4);

	/* ...unless diff is NULL */
	ret = parse_uint("1234a", -1, NULL, 0, 0, &out);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);

	/* numeric overflow is cought */
	ret = parse_uint("18446744073709551616", -1, NULL, 0, 0, &out);
	TEST_EQUAL_I(ret, SQFS_ERROR_OVERFLOW);

	/* buffer length is adherered to */
	ret = parse_uint("18446744073709551616", 5, NULL, 0, 0, &out);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(out, 18446);

	ret = parse_uint("18446744073709551616", 5, &diff, 0, 0, &out);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(diff, 5);
	TEST_EQUAL_UI(out, 18446);

	/* if vmin/vmax differ, check the range */
	ret = parse_uint("1234", -1, NULL, 0, 1000, &out);
	TEST_EQUAL_I(ret, SQFS_ERROR_OUT_OF_BOUNDS);

	ret = parse_uint("1234", -1, NULL, 0, 2000, &out);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_UI(out, 1234);

	ret = parse_uint("1234", -1, NULL, 2000, 3000, &out);
	TEST_EQUAL_I(ret, SQFS_ERROR_OUT_OF_BOUNDS);

	/* int version accepts '-' prefix */
	ret = parse_int("1234", -1, NULL, 0, 0, &s_out);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(s_out, 1234);

	ret = parse_int("-1234", -1, NULL, 0, 0, &s_out);
	TEST_EQUAL_I(ret, 0);
	TEST_EQUAL_I(s_out, -1234);

	ret = parse_int("- 1234", -1, NULL, 0, 0, &s_out);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);

	ret = parse_int("+1234", -1, NULL, 0, 0, &s_out);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);

	ret = parse_int("-1234", -1, NULL, -1000, 1000, &s_out);
	TEST_EQUAL_I(ret, SQFS_ERROR_OUT_OF_BOUNDS);

	return EXIT_SUCCESS;
}
