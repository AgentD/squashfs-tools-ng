/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_slink.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util/test.h"

int main(int argc, char **argv)
{
	fstree_defaults_t defaults;
	tree_node_t *node;
	struct stat sb;
	fstree_t fs;
	int ret;
	(void)argc; (void)argv;

	memset(&defaults, 0, sizeof(defaults));
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFLNK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_add_generic(&fs, "/symlink", &sb, "target");
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, S_IFLNK | 0777);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= node->name + 8);
	TEST_STR_EQUAL(node->name, "symlink");
	TEST_STR_EQUAL(node->data.target, "target");

	node = fstree_add_generic(&fs, "/foo", &sb, "");
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, S_IFLNK | 0777);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= node->name + 3);
	TEST_STR_EQUAL(node->name, "foo");
	TEST_STR_EQUAL(node->data.target, "");

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
