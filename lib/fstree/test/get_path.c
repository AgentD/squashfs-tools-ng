/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "common.h"
#include "util/test.h"

static dir_entry_t *mkentry(const char *name)
{
	dir_entry_t *ent = dir_entry_create(name);
	TEST_NOT_NULL(ent);
	ent->mode = S_IFDIR | 0750;
	ent->uid = 1000;
	ent->gid = 100;
	return ent;
}

int main(int argc, char **argv)
{
	tree_node_t *a, *b, *c, *d;
	fstree_defaults_t fsd;
	dir_entry_t *ent;
	fstree_t fs;
	char *str;
	(void)argc; (void)argv;

	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);
	TEST_ASSERT(fstree_init(&fs, &fsd) == 0);

	ent = mkentry("foo");
	a = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	ent = mkentry("foo/bar");
	b = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	ent = mkentry("foo/bar/baz");
	c = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	ent = mkentry("foo/bar/baz/dir");
	d = fstree_add_generic(&fs, ent, NULL);
	free(ent);

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
