/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * epoch.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/util.h"
#include "util/test.h"

#if defined(_WIN32) || defined(__WINDOWS__)
static void setenv(const char *key, const char *value, int overwrite)
{
	char buffer[128];
	(void)overwrite;

	snprintf(buffer, sizeof(buffer) - 1, "%s=%s", key, value);
	buffer[sizeof(buffer) - 1] = '\0';

	_putenv(buffer);
}

static void unsetenv(const char *key)
{
	setenv(key, "", 0);
}
#endif

int main(int argc, char **argv)
{
	sqfs_u32 ts;
	(void)argc; (void)argv;

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
