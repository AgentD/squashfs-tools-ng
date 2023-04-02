/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_cli.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "common.h"
#include "util/test.h"
#include "util/util.h"

int main(int argc, char **argv)
{
	fstree_defaults_t fs;
	char *str;
	(void)argc; (void)argv;

	str = strdup("mtime=1337,uid=1000,gid=100,mode=0321");
	TEST_NOT_NULL(str);
	TEST_ASSERT(parse_fstree_defaults(&fs, str) == 0);
	free(str);
	TEST_EQUAL_UI(fs.mtime, 1337);
	TEST_EQUAL_UI(fs.uid, 1000);
	TEST_EQUAL_UI(fs.gid, 100);
	TEST_EQUAL_UI(fs.mode, S_IFDIR | 0321);

	TEST_ASSERT(parse_fstree_defaults(&fs, NULL) == 0);
	if (fs.mtime != 0) {
		TEST_EQUAL_UI(fs.mtime, get_source_date_epoch());
	}
	TEST_EQUAL_UI(fs.uid, 0);
	TEST_EQUAL_UI(fs.gid, 0);
	TEST_EQUAL_UI(fs.mode, S_IFDIR | 0755);

	str = strdup("mode=07777");
	TEST_NOT_NULL(str);
	TEST_ASSERT(parse_fstree_defaults(&fs, str) == 0);
	free(str);

	str = strdup("mode=017777");
	TEST_NOT_NULL(str);
	TEST_ASSERT(parse_fstree_defaults(&fs, str) != 0);
	free(str);

	return EXIT_SUCCESS;
}
