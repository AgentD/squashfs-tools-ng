/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * split_line.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/parse.h"
#include "util/test.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>

static const struct {
	const char *in;
	size_t count;
	const char **out;
} split[] = {
	{ "", 0, NULL },
	{ "  \t  ", 0, NULL },
	{ "foo", 1, (const char *[]){ "foo" } },
	{ "   foo  ", 1, (const char *[]){ "foo" } },
	{ "foo bar", 2, (const char *[]){ "foo", "bar" } },
	{ "  foo \t bar  ", 2, (const char *[]){ "foo", "bar" } },
	{ "  foo \t bar  baz  ", 3, (const char *[]){ "foo", "bar", "baz" } },
	{ "  foo \t \"  bar  \"  baz  ", 3,
	  (const char *[]){ "foo", "  bar  ", "baz" } },
	{ "  foo \t \"  \\\"bar  \"  baz  ", 3,
	  (const char *[]){ "foo", "  \"bar  ", "baz" } },
	{ "  foo \t \"  bar  \\\\\"  baz  ", 3,
	  (const char *[]){ "foo", "  bar  \\", "baz" } },
};

static const struct {
	const char *in;
	size_t orig;
	size_t count;
	size_t remain;
	const char **out;
} drop_test[] = {
	{ "foo,bar,baz", 3, 0, 3, (const char *[]){ "foo", "bar", "baz" } },
	{ "foo,bar,baz", 3, 1, 2, (const char *[]){ "bar", "baz" } },
	{ "foo,bar,baz", 3, 2, 1, (const char *[]){ "baz" } },
	{ "foo,bar,baz", 3, 3, 0, NULL },
	{ "foo,bar,baz", 3, 4, 0, NULL },
	{ "foo,bar,baz", 3, 100, 0, NULL },
};

static void dump_components(split_line_t *sep)
{
	for (size_t i = 0; i < sep->count; ++i)
		fprintf(stderr, "\t`%s`\n", sep->args[i]);
}

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	for (size_t i = 0; i < sizeof(split) / sizeof(split[0]); ++i) {
		split_line_t *sep;
		char *copy;
		int ret;

		copy = strdup(split[i].in);
		TEST_NOT_NULL(copy);

		ret = split_line(copy, strlen(copy), " \t", &sep);
		TEST_EQUAL_I(ret, 0);
		TEST_NOT_NULL(sep);

		fprintf(stderr, "splitting `%s`\n", split[i].in);
		dump_components(sep);

		TEST_EQUAL_UI(sep->count, split[i].count);

		for (size_t j = 0; j < sep->count; ++j) {
			TEST_STR_EQUAL(sep->args[j], split[i].out[j]);
		}

		free(sep);
		free(copy);
	}

	for (size_t i = 0; i < sizeof(drop_test) / sizeof(drop_test[0]); ++i) {
		split_line_t *sep;
		char *copy;
		int ret;

		copy = strdup(drop_test[i].in);
		TEST_NOT_NULL(copy);

		fprintf(stderr, "splitting `%s`\n", drop_test[i].in);

		ret = split_line(copy, strlen(copy), ",", &sep);
		TEST_EQUAL_I(ret, 0);
		TEST_NOT_NULL(sep);

		dump_components(sep);

		TEST_EQUAL_UI(sep->count, drop_test[i].orig);

		fprintf(stderr, "removing first %u components\n",
			(unsigned int)drop_test[i].count);

		split_line_remove_front(sep, drop_test[i].count);
		dump_components(sep);

		TEST_EQUAL_UI(sep->count, drop_test[i].remain);

		for (size_t j = 0; j < sep->count; ++j) {
			TEST_STR_EQUAL(sep->args[j], drop_test[i].out[j]);
		}

		free(sep);
		free(copy);
	}

	return EXIT_SUCCESS;
}
