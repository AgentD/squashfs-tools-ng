/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_sparse_gnu2.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "test_tar.h"

int main(void)
{
	TEST_ASSERT(chdir(TEST_PATH) == 0);

	test_case_sparse("sparse-files/pax-gnu0-1.tar");
	test_case_sparse("sparse-files/pax-gnu1-0.tar");
	return EXIT_SUCCESS;
}
