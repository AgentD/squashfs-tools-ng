/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_ustar.c
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

	testcase_simple("format-acceptance/ustar.tar", 1542905892,
			1000, 1000, "input.txt");
	testcase_simple("format-acceptance/ustar-pre-posix.tar", 1542905892,
			1000, 1000, "input.txt");
	testcase_simple("format-acceptance/v7.tar", 1542905892,
			1000, 1000, "input.txt");
	testcase_simple("user-group-largenum/8-digit.tar", 1542995392,
			8388608, 8388608, "input.txt");
	testcase_simple("large-mtime/12-digit.tar", 8589934592L,
			1000, 1000, "input.txt");
	testcase_simple("long-paths/ustar.tar", 1542909670,
			1000, 1000, filename);

	test_case_file_size("file-size/12-digit.tar");
	return EXIT_SUCCESS;
}
