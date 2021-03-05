/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filename_sane.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"
#include "../test.h"

static const char *must_work[] = {
	"foobar",
	"test.txt",
#if !defined(_WIN32) && !defined(__WINDOWS__) && !defined(TEST_WIN32)
	"\\foo", "foo\\", "foo\\bar",
#endif
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

static const char *must_not_work_here[] = {
#if defined(_WIN32) || defined(__WINDOWS__) || defined(TEST_WIN32)
	"\\foo", "foo\\", "foo\\bar",
	"fo<o", "fo>o", "fo:o", "fo\"o",
	"fo|o", "fo?o", "fo*o", "fo\ro",
	"CON", "PRN", "AUX", "NUL",
	"COM1", "COM2", "LPT1", "LPT2",
	"con", "prn", "aux", "nul",
	"com1", "com2", "lpt1", "lpt2",
	"AUX.txt", "aux.txt", "NUL.txt", "nul.txt",
#endif
	NULL,
};

int main(void)
{
	size_t i;

	for (i = 0; must_work[i] != NULL; ++i) {
		if (!is_filename_sane(must_work[i], false)) {
			fprintf(stderr, "%s was rejected!\n", must_work[i]);
			return EXIT_FAILURE;
		}

		if (!is_filename_sane(must_work[i], true)) {
			fprintf(stderr,
				"%s was rejected when testing for "
				"OS specific stuff!\n", must_work[i]);
			return EXIT_FAILURE;
		}
	}

	for (i = 0; must_not_work[i] != NULL; ++i) {
		if (is_filename_sane(must_not_work[i], false)) {
			fprintf(stderr, "%s was accepted!\n",
				must_not_work[i]);
			return EXIT_FAILURE;
		}

		if (is_filename_sane(must_not_work[i], true)) {
			fprintf(stderr,
				"%s was accepted when testing for "
				"OS specific stuff!\n", must_not_work[i]);
			return EXIT_FAILURE;
		}
	}

	for (i = 0; must_not_work_here[i] != NULL; ++i) {
		if (!is_filename_sane(must_not_work_here[i], false)) {
			fprintf(stderr,
				"%s was rejected in the generic test!\n",
				must_not_work_here[i]);
			return EXIT_FAILURE;
		}

		if (is_filename_sane(must_not_work_here[i], true)) {
			fprintf(stderr,
				"%s was accepted when testing for "
				"OS specific stuff!\n", must_not_work_here[i]);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
