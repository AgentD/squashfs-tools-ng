/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_line.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstream.h"
#include "../test.h"

typedef struct {
	size_t line_num;
	const char *str;
} line_t;

static void run_test_case(const line_t *lines, size_t count,
			  int flags)
{
	size_t i, line_num, old_line_num;
	istream_t *fp;
	char *line;
	int ret;

	fp = istream_open_file(STRVALUE(TESTFILE));
	TEST_NOT_NULL(fp);

	line_num = 1;
	line = NULL;

	for (i = 0; i < count; ++i) {
		old_line_num = line_num;
		ret = istream_get_line(fp, &line, &line_num, flags);

		TEST_ASSERT(line_num >= old_line_num);
		TEST_EQUAL_I(ret, 0);
		TEST_NOT_NULL(line);

		TEST_EQUAL_UI(line_num, lines[i].line_num);
		TEST_STR_EQUAL(line, lines[i].str);

		free(line);
		line = NULL;
		line_num += 1;
	}

	ret = istream_get_line(fp, &line, &line_num, flags);
	TEST_ASSERT(ret > 0);

	sqfs_destroy(fp);
}

static const line_t lines_raw[] = {
	{ 1, "" },
	{ 2, "The quick" },
	{ 3, "  " },
	{ 4, "  brown fox  " },
	{ 5, "" },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 9, "" },
	{ 10, "dog" },
	{ 11, "" },
};

static const line_t lines_ltrim[] = {
	{ 1, "" },
	{ 2, "The quick" },
	{ 3, "" },
	{ 4, "brown fox  " },
	{ 5, "" },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 9, "" },
	{ 10, "dog" },
	{ 11, "" },
};

static const line_t lines_rtrim[] = {
	{ 1, "" },
	{ 2, "The quick" },
	{ 3, "" },
	{ 4, "  brown fox" },
	{ 5, "" },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 9, "" },
	{ 10, "dog" },
	{ 11, "" },
};

static const line_t lines_trim[] = {
	{ 1, "" },
	{ 2, "The quick" },
	{ 3, "" },
	{ 4, "brown fox" },
	{ 5, "" },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 9, "" },
	{ 10, "dog" },
	{ 11, "" },
};

static const line_t lines_no_empty[] = {
	{ 2, "The quick" },
	{ 3, "  " },
	{ 4, "  brown fox  " },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 10, "dog" },
};

static const line_t lines_no_empty_ltrim[] = {
	{ 2, "The quick" },
	{ 4, "brown fox  " },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 10, "dog" },
};

static const line_t lines_no_empty_rtrim[] = {
	{ 2, "The quick" },
	{ 4, "  brown fox" },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 10, "dog" },
};

static const line_t lines_no_empty_trim[] = {
	{ 2, "The quick" },
	{ 4, "brown fox" },
	{ 6, "jumps over" },
	{ 7, "the" },
	{ 8, "lazy" },
	{ 10, "dog" },
};

int main(void)
{
	run_test_case(lines_raw, 11, 0);
	run_test_case(lines_ltrim, 11, ISTREAM_LINE_LTRIM);
	run_test_case(lines_rtrim, 11, ISTREAM_LINE_RTRIM);
	run_test_case(lines_trim, 11,
		      ISTREAM_LINE_LTRIM | ISTREAM_LINE_RTRIM);

	run_test_case(lines_no_empty, 7, ISTREAM_LINE_SKIP_EMPTY);
	run_test_case(lines_no_empty_ltrim, 6,
		      ISTREAM_LINE_SKIP_EMPTY | ISTREAM_LINE_LTRIM);
	run_test_case(lines_no_empty_rtrim, 6,
		      ISTREAM_LINE_SKIP_EMPTY | ISTREAM_LINE_RTRIM);
	run_test_case(lines_no_empty_trim, 6,
		      ISTREAM_LINE_SKIP_EMPTY | ISTREAM_LINE_LTRIM |
		      ISTREAM_LINE_RTRIM);

	return EXIT_SUCCESS;
}
