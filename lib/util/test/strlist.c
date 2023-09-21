/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * strlist.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/util.h"
#include "util/test.h"
#include "util/strlist.h"

int main(int argc, char **argv)
{
	const char *str0, *str1, *str2;
	strlist_t a, b;
	int ret;
	(void)argc; (void)argv;

	str0 = "foo";
	str1 = "bar";
	str2 = "baz";

	/* init */
	strlist_init(&a);
	TEST_NULL(a.strings);
	TEST_EQUAL_UI(a.count, 0);
	TEST_EQUAL_UI(a.capacity, 0);

	/* append a string */
	ret = strlist_append(&a, str0);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(a.count, 1);
	TEST_ASSERT(a.capacity >= a.count);

	TEST_NOT_NULL(a.strings);
	TEST_NOT_NULL(a.strings[0]);

	TEST_STR_EQUAL(a.strings[0], str0);
	TEST_ASSERT(a.strings[0] != str0);

	/* append another */
	ret = strlist_append(&a, str1);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(a.count, 2);
	TEST_ASSERT(a.capacity >= a.count);

	TEST_NOT_NULL(a.strings);
	TEST_NOT_NULL(a.strings[0]);
	TEST_NOT_NULL(a.strings[1]);

	TEST_ASSERT(a.strings[0] != str0);
	TEST_ASSERT(a.strings[1] != str1);
	TEST_STR_EQUAL(a.strings[0], str0);
	TEST_STR_EQUAL(a.strings[1], str1);

	/* and one more */
	ret = strlist_append(&a, str2);
	TEST_EQUAL_I(ret, 0);

	TEST_EQUAL_UI(a.count, 3);
	TEST_ASSERT(a.capacity >= a.count);

	TEST_NOT_NULL(a.strings);
	TEST_NOT_NULL(a.strings[0]);
	TEST_NOT_NULL(a.strings[1]);
	TEST_NOT_NULL(a.strings[2]);

	TEST_ASSERT(a.strings[0] != str0);
	TEST_ASSERT(a.strings[1] != str1);
	TEST_ASSERT(a.strings[2] != str2);
	TEST_STR_EQUAL(a.strings[0], str0);
	TEST_STR_EQUAL(a.strings[1], str1);
	TEST_STR_EQUAL(a.strings[2], str2);

	/* copy */
	strlist_init_copy(&b, &a);
	TEST_NOT_NULL(b.strings);
	TEST_EQUAL_UI(b.count, a.count);
	TEST_EQUAL_UI(b.capacity, a.capacity);

	TEST_ASSERT(b.strings != a.strings);

	TEST_NOT_NULL(b.strings[0]);
	TEST_NOT_NULL(b.strings[1]);
	TEST_NOT_NULL(b.strings[2]);

	TEST_ASSERT(b.strings[0] != a.strings[0]);
	TEST_ASSERT(b.strings[1] != a.strings[1]);
	TEST_ASSERT(b.strings[2] != a.strings[2]);

	TEST_STR_EQUAL(b.strings[0], str0);
	TEST_STR_EQUAL(b.strings[1], str1);
	TEST_STR_EQUAL(b.strings[2], str2);

	/* cleanup */
	strlist_cleanup(&a);
	strlist_cleanup(&b);

	TEST_NULL(a.strings);
	TEST_NULL(b.strings);

	TEST_EQUAL_UI(a.count, 0);
	TEST_EQUAL_UI(b.count, 0);

	TEST_EQUAL_UI(a.capacity, 0);
	TEST_EQUAL_UI(b.capacity, 0);
	return EXIT_SUCCESS;
}
