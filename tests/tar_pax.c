/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_pax.c
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

	testcase_simple("format-acceptance/pax.tar", 1542905892,
			1000, 1000, "input.txt");
	testcase_simple("user-group-largenum/pax.tar", 013376036700,
			2147483648UL, 2147483648UL, "input.txt");
	testcase_simple("large-mtime/pax.tar", 8589934592L,
			1000, 1000, "input.txt");
	testcase_simple("negative-mtime/pax.tar", -315622800,
			1000, 1000, "input.txt");
	testcase_simple("long-paths/pax.tar", 1542909670,
			1000, 1000, filename);

	test_case_file_size("file-size/pax.tar");
	return EXIT_SUCCESS;
}
