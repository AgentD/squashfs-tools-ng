/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * base64_decode.c
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
	{ 0, "", "" },
	{ 0, "Zg", "f" },
	{ 0, "Zg==", "f" },
	{ 0, "Zm8=", "fo" },
	{ 0, "Zm9v", "foo" },
	{ 0, "Zm9vYg==", "foob" },
	{ 0, "Zm9vYmE=", "fooba" },
	{ 0, "Zm9vYmFy", "foobar" },
	{ 0, "TGV0J3MgYWxsIGxvdmUgTGFpbiEK", "Let's all love Lain!\n" },
	{ -1, "Zg==X",     "XX" },
};

int main(int argc, char **argv)
{
	sqfs_u8 buffer[256];
	size_t i, j;
	(void)argc; (void)argv;

	for (i = 0; i < sizeof(test_vec) / sizeof(test_vec[0]); ++i) {
		const size_t in_len = strlen(test_vec[i].in);
		const size_t out_len = strlen(test_vec[i].out);
		size_t real_out;
		int ret;

		/* initialize the buffer */
		for (j = 0; j < sizeof(buffer); ++j) {
			buffer[j] = (j % 2) ? 0xAA : 0x55;
		}

		/* convert */
		real_out = sizeof(buffer);
		ret = base64_decode(test_vec[i].in, in_len, buffer, &real_out);

		/* make sure pattern is un-touched after expected offset */
		j = (in_len / 4) * 3;
		if (in_len % 4)
			j += 3;

		for (; j < sizeof(buffer); ++j) {
			TEST_ASSERT(buffer[j] == ((j % 2) ? 0xAA : 0x55));
		}

		/* check result */
		if (test_vec[i].result == 0) {
			TEST_ASSERT(ret == 0);
			TEST_EQUAL_UI(real_out, out_len);
			ret = memcmp(buffer, test_vec[i].out, out_len);
			TEST_ASSERT(ret == 0);
		} else {
			TEST_ASSERT(ret != 0);
			TEST_EQUAL_UI(real_out, 0);
		}

		fprintf(stderr, "CASE %lu OK\n", (unsigned long)i);
	}

	return EXIT_SUCCESS;
}

