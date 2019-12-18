/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filename_sane.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

static const char *must_work[] = {
	"foobar",
	"test.txt",
	NULL,
};

static const char *must_not_work[] = {
	".",
	"..",
	"/foo",
	"\\foo",
	"foo/",
	"foo\\",
	"foo/bar",
	"foo\\bar",
	NULL,
};

static const char *must_not_work_here[] = {
#if defined(_WIN32) || defined(__WINDOWS__)
	"fo<o", "fo>o", "fo:o", "fo\"o",
	"fo|o", "fo?o", "fo*o", "fo\ro",
	"CON", "PRN", "AUX", "NUL",
	"COM1", "COM2", "LPT1", "LPT2",
	"con", "prn", "aux", "nul",
	"com1", "com2", "lpt1", "lpt2",
	"foo.AUX", "foo.NUL", "foo.aux", "foo.nul",
	"NUL.txt", "nul.txt"
#endif
	NULL,
};

int main(void)
{
	size_t i;

	for (i = 0; must_work[i] != NULL; ++i) {
		assert(is_filename_sane(must_work[i], false));
		assert(is_filename_sane(must_work[i], true));
	}

	for (i = 0; must_not_work[i] != NULL; ++i) {
		assert(!is_filename_sane(must_not_work[i], false));
		assert(!is_filename_sane(must_not_work[i], true));
	}

	for (i = 0; must_not_work_here[i] != NULL; ++i) {
		assert( is_filename_sane(must_not_work_here[i], false));
		assert(!is_filename_sane(must_not_work_here[i], true));
	}

	return EXIT_SUCCESS;
}
