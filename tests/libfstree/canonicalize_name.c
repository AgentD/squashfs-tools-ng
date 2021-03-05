/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * canonicalize_name.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"
#include "../test.h"

static const struct {
	const char *in;
	const char *out;
} must_work[] = {
	{ "", "" },
	{ "/", "" },
	{ "\\", "\\" },
	{ "///", "" },
	{ "\\\\\\", "\\\\\\" },
	{ "/\\//\\\\/", "\\/\\\\" },
	{ "foo/bar/test", "foo/bar/test" },
	{ "foo\\bar\\test", "foo\\bar\\test" },
	{ "/foo/bar/test/", "foo/bar/test" },
	{ "\\foo\\bar\\test\\", "\\foo\\bar\\test\\" },
	{ "///foo//bar//test///", "foo/bar/test" },
	{ "./foo/././bar/test/./.", "foo/bar/test" },
	{ "./foo/././", "foo" },
	{ ".", "" },
	{ "./", "" },
	{ "./.", "" },
	{ "foo/.../bar", "foo/.../bar" },
	{ "foo/.test/bar", "foo/.test/bar" },
};

static const char *must_not_work[] = {
	"..",
	"foo/../bar",
	"../foo/bar",
	"foo/bar/..",
	"foo/bar/../",
};

int main(void)
{
	char buffer[512];
	size_t i;

	for (i = 0; i < sizeof(must_work) / sizeof(must_work[0]); ++i) {
		strcpy(buffer, must_work[i].in);

		if (canonicalize_name(buffer)) {
			fprintf(stderr, "Test case rejected: '%s'\n",
				must_work[i].in);
			return EXIT_FAILURE;
		}

		if (strcmp(buffer, must_work[i].out) != 0) {
			fprintf(stderr, "Expected result: %s\n",
				must_work[i].out);
			fprintf(stderr, "Actual result: %s\n", buffer);
			return EXIT_FAILURE;
		}
	}

	for (i = 0; i < sizeof(must_not_work) / sizeof(must_not_work[0]); ++i) {
		strcpy(buffer, must_not_work[i]);

		if (canonicalize_name(buffer) == 0) {
			fprintf(stderr, "Test case accepted: '%s'\n",
				must_not_work[i]);
			fprintf(stderr, "Transformed into: '%s'\n", buffer);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
