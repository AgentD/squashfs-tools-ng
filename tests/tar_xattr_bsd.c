/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_xattr_bsd.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test_tar.h"

int main(void)
{
	TEST_ASSERT(chdir(TEST_PATH) == 0);

	test_case_xattr_simple("xattr/xattr-libarchive.tar");
	return EXIT_SUCCESS;
}
