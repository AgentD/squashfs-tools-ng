/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_line.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "io/ostream.h"
#include "util/test.h"
#include "util/util.h"

static ostream_t dummy;
static size_t total = 0;

static int dummy_append(ostream_t *strm, const void *data, size_t size)
{
	bool bret;

	TEST_ASSERT(strm == &dummy);
	TEST_NOT_NULL(data);
	TEST_ASSERT(size > 0);

	bret = is_memory_zero(data, size);
	TEST_ASSERT(bret);

	bret = SZ_ADD_OV(total, size, &total);
	TEST_ASSERT(!bret);
	return 0;
}

static int dummy_flush(ostream_t *strm)
{
	TEST_ASSERT(strm == &dummy);
	return 0;
}

static ostream_t dummy = {
	{
		1,
		NULL,
		NULL,
	},
	dummy_append,
	NULL,
	dummy_flush,
	NULL,
};

int main(int argc, char **argv)
{
	size_t ref;
	int ret;
	(void)argc; (void)argv;

	ref = 131072 + 1337;

	ret = ostream_append_sparse(&dummy, ref);
	TEST_EQUAL_I(ret, 0);

	ret = dummy.flush(&dummy);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(ref, total);
	return EXIT_SUCCESS;
}
