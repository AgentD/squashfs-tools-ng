/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_simple.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "../test.h"

int main(void)
{
	tree_node_t *node;
	struct stat sb;
	fstree_t fs;

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFSOCK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(NULL, "sockfile", 8, NULL, &sb);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "sockfile");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->parent);
	TEST_NULL(node->data.target);
	TEST_EQUAL_UI(node->data.devno, 0);
	free(node);

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFIFO | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(NULL, "fifo", 4, NULL, &sb);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "fifo");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->parent);
	TEST_NULL(node->data.target);
	TEST_EQUAL_UI(node->data.devno, 0);
	free(node);

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(NULL, "blkdev", 6, NULL, &sb);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "blkdev");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_EQUAL_UI(node->data.devno, sb.st_rdev);
	TEST_NULL(node->parent);
	free(node);

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFCHR | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(NULL, "chardev", 7, NULL, &sb);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "chardev");
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, sb.st_mode);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_EQUAL_UI(node->data.devno, sb.st_rdev);
	TEST_NULL(node->parent);
	free(node);

	return EXIT_SUCCESS;
}
