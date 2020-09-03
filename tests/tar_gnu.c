/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_gnu.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test_tar.h"

static const char *filename =
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/012345678901234567890123456789/"
"012345678901234567890123456789/input.txt";

int main(void)
{
	TEST_ASSERT(chdir(TEST_PATH) == 0);

	testcase_simple("format-acceptance/gnu.tar", 1542905892,
			1000, 1000, "input.txt");
	testcase_simple("format-acceptance/gnu-g.tar", 013375560044,
			1000, 1000, "input.txt");
	testcase_simple("user-group-largenum/gnu.tar", 013376036700,
			0x80000000, 0x80000000, "input.txt");
	testcase_simple("negative-mtime/gnu.tar", -315622800,
			1000, 1000, "input.txt");
	testcase_simple("long-paths/gnu.tar", 1542909670,
			1000, 1000, filename);
	testcase_simple("large-mtime/gnu.tar", 8589934592L,
			1000, 1000, "input.txt");
	test_case_file_size("file-size/gnu.tar");
	return EXIT_SUCCESS;
}
