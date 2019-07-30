/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * crc32.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
	const char *str;
	uint32_t result;
} test_vectors[] = {
	{ "", 0 },
	{ "Hello, World!", 0xEC4AC3D0 },
	{ "The quick brown fox jumps over the lazy dog", 0x414FA339 },
};

int main(void)
{
	uint32_t crc;
	size_t i;

	for (i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); ++i) {
		crc = update_crc32(0, test_vectors[i].str,
				   strlen(test_vectors[i].str));

		if (crc != test_vectors[i].result) {
			fprintf(stderr, "Case %zu failed: %08X != %08X\n", i,
				crc, test_vectors[i].result);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
