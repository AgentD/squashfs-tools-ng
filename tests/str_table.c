/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * str_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "str_table.h"
#include "compat.h"
#include "test.h"

#define STR(x) #x
#define STRVALUE(x) STR(x)

#define TEST_PATH STRVALUE(TESTPATH)

static char *strings[1000];

static int read_strings(void)
{
	ssize_t ret;
	char *line;
	size_t n;
	FILE *fp;
	int i;

	fp = test_open_read("words.txt");

	for (i = 0; i < 1000; ++i) {
		line = NULL;
		n = 0;

		ret = getline(&line, &n, fp);
		if (ret < 0) {
			perror("reading words");
			goto fail;
		}

		strings[i] = line;
	}

	fclose(fp);
	return 0;
fail:
	for (i = 0; i < 1000; ++i)
		free(strings[i]);
	fclose(fp);
	return -1;
}

int main(void)
{
	str_table_t table;
	size_t i, j, idx;
	const char *str;

	TEST_ASSERT(chdir(TEST_PATH) == 0);

	if (read_strings())
		return EXIT_FAILURE;

	TEST_ASSERT(str_table_init(&table, 64) == 0);

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
