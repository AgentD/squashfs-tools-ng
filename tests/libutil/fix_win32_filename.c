/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fix_win32_filename.c
 *
 * Copyright (C) 2024 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "util/util.h"

static const struct {
	const char *path;
	const char *result;
} test_data[] = {
	{ "foo", "foo" },
	{ "foo/bar", "foo/bar" },
	{ "foo/bar.txt", "foo/bar.txt" },
	{ "COM1", "COM1_" },
	{ "COM1.txt", "COM1_.txt" },
	{ "foo.aux", "foo.aux_" },
	{ "foo/bar/test.LPT1/bla", "foo/bar/test.LPT1_/bla" },
	{ "C:\\/foo/COM1.bla/bar",
	  "C\xEF\x80\xBA\xEF\x81\x9c/foo/COM1_.bla/bar" },
};

int main(int argc, char **argv)
{
	(void)argc; (void)argv;

	for (size_t i = 0; i < sizeof(test_data) / sizeof(test_data[0]); ++i) {
		char *result = fix_win32_filename(test_data[i].path);
		size_t out_len = strlen(test_data[i].result);

		if (result == NULL) {
			fprintf(stderr, "OOM for test case %u (%s)?\n",
				(unsigned int)i, test_data[i].path);
			return EXIT_FAILURE;
		}

		if (out_len != strlen(result) ||
		    memcmp(result, test_data[i].result, out_len) != 0) {
			fprintf(stderr,
				"Mismatch for %s -> %s, got %s instead!\n",
				test_data[i].path, test_data[i].result,
				result);
			free(result);
			return EXIT_FAILURE;
		}

		free(result);
	}

	return EXIT_SUCCESS;
}
