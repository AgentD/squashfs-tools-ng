/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_init.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "internal.h"
#include "../test.h"

int main(void)
{
	fstree_t fs;
	char *str;

	str = strdup("mtime=1337,uid=1000,gid=100,mode=0321");
	TEST_NOT_NULL(str);
	TEST_ASSERT(fstree_init(&fs, str) == 0);
	free(str);
	TEST_EQUAL_UI(fs.defaults.st_mtime, 1337);
	TEST_EQUAL_UI(fs.defaults.st_uid, 1000);
	TEST_EQUAL_UI(fs.defaults.st_gid, 100);
	TEST_EQUAL_UI(fs.defaults.st_mode, S_IFDIR | 0321);
	fstree_cleanup(&fs);

	TEST_ASSERT(fstree_init(&fs, NULL) == 0);
	if (fs.defaults.st_mtime != 0) {
		TEST_EQUAL_UI(fs.defaults.st_mtime, get_source_date_epoch());
	}
	TEST_EQUAL_UI(fs.defaults.st_uid, 0);
	TEST_EQUAL_UI(fs.defaults.st_gid, 0);
	TEST_EQUAL_UI(fs.defaults.st_mode, S_IFDIR | 0755);
	fstree_cleanup(&fs);

	str = strdup("mode=07777");
	TEST_NOT_NULL(str);
	TEST_ASSERT(fstree_init(&fs, str) == 0);
	free(str);
	fstree_cleanup(&fs);

	str = strdup("mode=017777");
	TEST_NOT_NULL(str);
	TEST_ASSERT(fstree_init(&fs, str) != 0);
	free(str);

	return EXIT_SUCCESS;
}
