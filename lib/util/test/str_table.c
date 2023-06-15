/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * str_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/str_table.h"
#include "compat.h"
#include "io/istream.h"
#include "util/test.h"
#include "sqfs/io.h"

static char *strings[1000];

static int read_strings(void)
{
	sqfs_istream_t *fp;
	ssize_t ret;
	char *line;
	int i;

	i = sqfs_istream_open_file(&fp, "words.txt", 0);
	TEST_EQUAL_I(i, 0);
	TEST_NOT_NULL(fp);

	for (i = 0; i < 1000; ++i) {
		ret = istream_get_line(fp, &line, NULL, 0);
		TEST_EQUAL_I(ret, 0);

		strings[i] = line;
	}

	sqfs_drop(fp);
	return 0;
}

int main(int argc, char **argv)
{
	str_table_t table;
	size_t i, j, idx;
	const char *str;
	(void)argc; (void)argv;

	TEST_ASSERT(chdir(TEST_PATH) == 0);

	if (read_strings())
		return EXIT_FAILURE;

	TEST_ASSERT(str_table_init(&table) == 0);

	for (i = 0; i < 1000; ++i) {
		TEST_ASSERT(str_table_get_index(&table, strings[i], &idx) == 0);

		TEST_EQUAL_UI(idx, i);

		for (j = 0; j <= i; ++j) {
			str = str_table_get_string(&table, j);

			TEST_NOT_NULL(str);
			TEST_ASSERT(str != strings[i]);
			TEST_STR_EQUAL(str, strings[j]);
		}

		for (; j < 1000; ++j)
			TEST_NULL(str_table_get_string(&table, j));
	}

	for (i = 0; i < 1000; ++i) {
		TEST_ASSERT(str_table_get_index(&table, strings[i], &idx) == 0);
		TEST_EQUAL_UI(idx, i);

		str = str_table_get_string(&table, i);

		TEST_NOT_NULL(str);
		TEST_ASSERT(str != strings[i]);
		TEST_STR_EQUAL(str, strings[i]);
	}

	str_table_cleanup(&table);

	for (i = 0; i < 1000; ++i)
		free(strings[i]);

	return EXIT_SUCCESS;
}
