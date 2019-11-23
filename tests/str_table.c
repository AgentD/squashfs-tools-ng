/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * str_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "util/str_table.h"
#include "compat.h"

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

	fp = fopen("words.txt", "r");

	if (fp == NULL) {
		perror("words.txt");
		return -1;
	}

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

	assert(chdir(TEST_PATH) == 0);

	if (read_strings())
		return EXIT_FAILURE;

	assert(str_table_init(&table, 64) == 0);

	for (i = 0; i < 1000; ++i) {
		assert(str_table_get_index(&table, strings[i], &idx) == 0);

		assert(idx == i);

		for (j = 0; j <= i; ++j) {
			str = str_table_get_string(&table, j);

			assert(str != NULL);
			assert(str != strings[i]);
			assert(strcmp(str, strings[j]) == 0);
		}

		for (; j < 1000; ++j) {
			str = str_table_get_string(&table, j);
			assert(str == NULL);
		}
	}

	for (i = 0; i < 1000; ++i) {
		assert(str_table_get_index(&table, strings[i], &idx) == 0);
		assert(idx == i);

		str = str_table_get_string(&table, i);

		assert(str != NULL);
		assert(str != strings[i]);
		assert(strcmp(str, strings[i]) == 0);
	}

	str_table_cleanup(&table);

	for (i = 0; i < 1000; ++i)
		free(strings[i]);

	return EXIT_SUCCESS;
}
