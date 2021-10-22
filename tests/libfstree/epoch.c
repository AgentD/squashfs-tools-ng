/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * epoch.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "internal.h"
#include "../test.h"

int main(void)
{
	sqfs_u32 ts;

	unsetenv("SOURCE_DATE_EPOCH");
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 0);

	setenv("SOURCE_DATE_EPOCH", "1337", 1);
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 1337);

	setenv("SOURCE_DATE_EPOCH", "0xCAFE", 1);
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 0);

	setenv("SOURCE_DATE_EPOCH", "foobar", 1);
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 0);

	setenv("SOURCE_DATE_EPOCH", "-12", 1);
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 0);

	setenv("SOURCE_DATE_EPOCH", "12", 1);
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 12);

	setenv("SOURCE_DATE_EPOCH", "4294967295", 1);
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 0xFFFFFFFF);

	setenv("SOURCE_DATE_EPOCH", "4294967296", 1);
	ts = get_source_date_epoch();
	TEST_EQUAL_UI(ts, 0);

	return EXIT_SUCCESS;
}
