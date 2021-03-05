/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "../test.h"

int main(void)
{
	tree_node_t *a, *b, *c, *d;
	struct stat sb;
	fstree_t fs;
	char *str;

	TEST_ASSERT(fstree_init(&fs, NULL) == 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0750;
	sb.st_uid = 1000;
	sb.st_gid = 100;

	a = fstree_add_generic(&fs, "foo", &sb, NULL);
	b = fstree_add_generic(&fs, "foo/bar", &sb, NULL);
	c = fstree_add_generic(&fs, "foo/bar/baz", &sb, NULL);
	d = fstree_add_generic(&fs, "foo/bar/baz/dir", &sb, NULL);

	str = fstree_get_path(fs.root);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/");
	free(str);

	str = fstree_get_path(a);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/foo");
	free(str);

	str = fstree_get_path(b);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/foo/bar");
	free(str);

	str = fstree_get_path(c);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/foo/bar/baz");
	free(str);

	str = fstree_get_path(d);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/foo/bar/baz/dir");
	free(str);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
