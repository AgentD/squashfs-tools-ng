/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * hex_decode.c
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/util.h"
#include "util/test.h"

static const struct {
	int result;
	const char *in;
	const char *out;
} test_vec[] = {
	{ 0, "", NULL },
	{ -1, "A", NULL },
	{ 0, "AA", "\xAA" },
	{ 0, "0A", "\x0A" },
	{ 0, "A0", "\xA0" },
	{ -1, "A0B", NULL },
	{ 0, "A0BC", "\xA0\xBC" },
	{ 0, "0123456789ABCDEF", "\x01\x23\x45\x67\x89\xAB\xCD\xEF" },
	{ 0, "0123456789abcdef", "\x01\x23\x45\x67\x89\xAB\xCD\xEF" },
	{ -1, "0123456789ABCDEFGH", NULL },
	{ -1, "0123456789abcdefgh", NULL },
};

int main(int argc, char **argv)
{
	sqfs_u8 buffer[256];
	size_t i, j;
	(void)argc; (void)argv;

	for (i = 0; i < sizeof(test_vec) / sizeof(test_vec[0]); ++i) {
		size_t in_len = strlen(test_vec[i].in);
		size_t out_len = in_len / 2;
		int ret;

		/* initialize the buffer */
		for (j = 0; j < sizeof(buffer); ++j) {
			buffer[j] = (j % 2) ? 0xAA : 0x55;
		}

		/* convert */
		ret = hex_decode(test_vec[i].in, in_len,
				 buffer, sizeof(buffer));

		/* make sure pattern is un-touched after expected offset */
		for (j = out_len; j < sizeof(buffer); ++j) {
			TEST_ASSERT(buffer[j] == ((j % 2) ? 0xAA : 0x55));
		}

		/* check result */
		if (test_vec[i].result == 0) {
			TEST_ASSERT(ret == 0);
			ret = memcmp(buffer, test_vec[i].out, out_len);
			TEST_ASSERT(ret == 0);
		} else {
			TEST_ASSERT(ret != 0);
		}
	}

	return EXIT_SUCCESS;
}

