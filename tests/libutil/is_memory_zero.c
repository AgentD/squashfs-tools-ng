/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * is_memory_zero.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "../test.h"
#include "util.h"

int main(void)
{
	unsigned char temp[1024];
	size_t i, j;

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
