/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filename_sane.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/util.h"
#include "util/test.h"

static const char *must_work[] = {
	"foobar",
	"test.txt",
	NULL,
};

static const char *must_not_work[] = {
	".",
	"..",
	"/foo",
	"foo/",
	"foo/bar",
	NULL,
};

int main(int argc, char **argv)
{
	size_t i;
	(void)argc; (void)argv;

	for (i = 0; must_work[i] != NULL; ++i) {
		if (!is_filename_sane(must_work[i])) {
			fprintf(stderr, "%s was rejected!\n", must_work[i]);
			return EXIT_FAILURE;
		}
	}

	for (i = 0; must_not_work[i] != NULL; ++i) {
		if (is_filename_sane(must_not_work[i])) {
			fprintf(stderr, "%s was accepted!\n",
				must_not_work[i]);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
