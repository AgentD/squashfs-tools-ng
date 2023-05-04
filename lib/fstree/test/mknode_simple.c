/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_simple.c
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
	sb.st_mode = S_IFSOCK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_add_generic(&fs, "/sockfile", &sb, "target");
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "sockfile");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->data.target);
	TEST_EQUAL_UI(node->data.devno, 0);

	fstree_cleanup(&fs);
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFIFO | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_add_generic(&fs, "/fifo", &sb, "target");
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "fifo");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->data.target);
	TEST_EQUAL_UI(node->data.devno, 0);

	fstree_cleanup(&fs);
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_add_generic(&fs, "/blkdev", &sb, NULL);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "blkdev");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_EQUAL_UI(node->data.devno, sb.st_rdev);

	fstree_cleanup(&fs);
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFCHR | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_add_generic(&fs, "/chardev", &sb, NULL);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "chardev");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_EQUAL_UI(node->data.devno, sb.st_rdev);
	fstree_cleanup(&fs);

	return EXIT_SUCCESS;
}
