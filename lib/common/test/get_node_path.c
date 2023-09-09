/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_node_path.c
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"
#include "util/test.h"

#include "sqfs/dir_reader.h"
#include "sqfs/error.h"
#include "dir_tree.h"

int main(int argc, char **argv)
{
	sqfs_tree_node_t *n0, *n1, *n2;
	char *str;
	int ret;
	(void)argc;
	(void)argv;

	n0 = calloc(1, sizeof(*n0) + 16);
	TEST_NOT_NULL(n0);

	n1 = calloc(1, sizeof(*n1) + 16);
	TEST_NOT_NULL(n1);

	n2 = calloc(1, sizeof(*n2) + 16);
	TEST_NOT_NULL(n2);

	/* no parent -> must return "/" */
	ret = sqfs_tree_node_get_path(n0, &str);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/");
	sqfs_free(str);

	/* hiearchy levels */
	n1->parent = n0;
	n0->children = n1;
	strcpy((char *)n1->name, "bar");

	n2->parent = n1;
	n1->children = n2;
	strcpy((char *)n2->name, "baz");

	ret = sqfs_tree_node_get_path(n1, &str);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/bar");
	sqfs_free(str);

	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/bar/baz");
	sqfs_free(str);

	/* root node must not have a name */
	strcpy((char *)n0->name, "foo");

	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, SQFS_ERROR_ARG_INVALID);
	TEST_NULL(str);
	n0->name[0] = '\0';

	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/bar/baz");
	sqfs_free(str);

	/* non-root nodes must have names */
	n1->name[0] = '\0';

	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);
	TEST_NULL(str);
	n1->name[0] = 'b';

	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/bar/baz");
	sqfs_free(str);

	/* some names are illegal */
	strcpy((char *)n1->name, "..");
	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);
	TEST_NULL(str);

	strcpy((char *)n1->name, ".");
	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);
	TEST_NULL(str);

	strcpy((char *)n1->name, "a/b");
	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, SQFS_ERROR_CORRUPTED);
	TEST_NULL(str);

	strcpy((char *)n1->name, "bar");
	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/bar/baz");
	sqfs_free(str);

	/* link loops must be detected */
	n0->parent = n2;
	strcpy((char *)n0->name, "foo");

	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, SQFS_ERROR_LINK_LOOP);
	TEST_NULL(str);

	n0->parent = NULL;
	n0->name[0] = '\0';

	ret = sqfs_tree_node_get_path(n2, &str);
	TEST_EQUAL_I(ret, 0);
	TEST_NOT_NULL(str);
	TEST_STR_EQUAL(str, "/bar/baz");
	sqfs_free(str);

	/* cleanup */
	free(n0);
	free(n1);
	free(n2);
	return EXIT_SUCCESS;
}
