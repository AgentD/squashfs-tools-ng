/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_slink.c
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
	sb.st_mode = S_IFLNK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(NULL, "symlink", 7, "target", &sb);
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, S_IFLNK | 0777);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->parent);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= node->name + 8);
	TEST_STR_EQUAL(node->name, "symlink");
	TEST_STR_EQUAL(node->data.target, "target");
	free(node);

	node = fstree_mknode(NULL, "symlink", 7, "", &sb);
	TEST_EQUAL_UI(node->uid, sb.st_uid);
	TEST_EQUAL_UI(node->gid, sb.st_gid);
	TEST_EQUAL_UI(node->mode, S_IFLNK | 0777);
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->parent);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= node->name + 8);
	TEST_STR_EQUAL(node->name, "symlink");
	TEST_STR_EQUAL(node->data.target, "");
	free(node);

	return EXIT_SUCCESS;
}
