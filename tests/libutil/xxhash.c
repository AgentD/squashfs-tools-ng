/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * xxhash.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util.h"
#include "../test.h"

static const struct {
	const char *plaintext;
	size_t psize;
	sqfs_u32 digest;
} test_vectors[] = {
	{
		.plaintext = "\x9e",
		.psize = 1,
		.digest = 0xB85CBEE5,
	},
	{
		.plaintext = "\x9e\xff\x1f\x4b\x5e\x53\x2f\xdd"
		"\xb5\x54\x4d\x2a\x95\x2b",
		.psize = 14,
		.digest = 0xE5AA0AB4,
	},
	{
		.plaintext = "\x9e\xff\x1f\x4b\x5e\x53\x2f\xdd"
		"\xb5\x54\x4d\x2a\x95\x2b\x57\xae"
		"\x5d\xba\x74\xe9\xd3\xa6\x4c\x98"
		"\x30\x60\xc0\x80\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00",
		.psize = 101,
		.digest = 0x018F52BC,
	},
};

int main(void)
{
	sqfs_u32 hash;
	size_t i;

	for (i = 0; i < sizeof(test_vectors) / sizeof(test_vectors[0]); ++i) {
		hash = xxh32(test_vectors[i].plaintext, test_vectors[i].psize);

		if (hash != test_vectors[i].digest) {
			fprintf(stderr, "Test case " PRI_SZ " failed!\n", i);
			fprintf(stderr, "Expected result: 0x%08X\n",
				test_vectors[i].digest);
			fprintf(stderr, "Actual result:   0x%08X\n", hash);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}
