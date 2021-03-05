/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "../test.h"

int main(void)
{
	tree_node_t *root, *a, *b;
	struct stat sb;
	fstree_t fs;

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 4096;

	root = fstree_mknode(NULL, "rootdir", 7, NULL, &sb);
	TEST_EQUAL_UI(root->uid, sb.st_uid);
	TEST_EQUAL_UI(root->gid, sb.st_gid);
	TEST_EQUAL_UI(root->mode, sb.st_mode);
	TEST_EQUAL_UI(root->link_count, 2);
	TEST_ASSERT(root->name >= (char *)root->payload);
	TEST_STR_EQUAL(root->name, "rootdir");
	TEST_NULL(root->data.dir.children);
	TEST_NULL(root->parent);
	TEST_NULL(root->next);

	a = fstree_mknode(root, "adir", 4, NULL, &sb);
	TEST_ASSERT(a->parent == root);
	TEST_NULL(a->next);
	TEST_EQUAL_UI(a->link_count, 2);
	TEST_EQUAL_UI(root->link_count, 3);
	TEST_ASSERT(root->data.dir.children == a);
	TEST_NULL(root->parent);
	TEST_NULL(root->next);

	b = fstree_mknode(root, "bdir", 4, NULL, &sb);
	TEST_ASSERT(a->parent == root);
	TEST_ASSERT(b->parent == root);
	TEST_EQUAL_UI(b->link_count, 2);
	TEST_ASSERT(root->data.dir.children == b);
	TEST_EQUAL_UI(root->link_count, 4);
	TEST_ASSERT(b->next == a);
	TEST_NULL(a->next);
	TEST_NULL(root->parent);
	TEST_NULL(root->next);

	free(root);
	free(a);
	free(b);

	return EXIT_SUCCESS;
}
