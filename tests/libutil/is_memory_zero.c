/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * is_memory_zero.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "util/util.h"

int main(int argc, char **argv)
{
	unsigned char temp[1024];
	size_t i, j;
	(void)argc; (void)argv;

	memset(temp, 0, sizeof(temp));

	for (i = 0; i < sizeof(temp); ++i) {
		TEST_ASSERT(is_memory_zero(temp, i));

		for (j = 0; j < i; ++j) {
			TEST_ASSERT(is_memory_zero(temp, i));
			temp[j] = 42;
			TEST_ASSERT(!is_memory_zero(temp, i));
			temp[j] = 0;
			TEST_ASSERT(is_memory_zero(temp, i));
		}
	}

	return EXIT_SUCCESS;
}
