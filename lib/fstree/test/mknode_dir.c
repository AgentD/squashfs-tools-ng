/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util/test.h"

int main(int argc, char **argv)
{
	fstree_defaults_t defaults;
	tree_node_t *root, *a, *b;
	struct stat sb;
	fstree_t fs;
	int ret;
	(void)argc; (void)argv;

	memset(&defaults, 0, sizeof(defaults));
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 4096;

	root = fstree_add_generic(&fs, "rootdir", &sb, NULL);
	TEST_NOT_NULL(root);
	TEST_ASSERT(root->parent == fs.root);
	TEST_EQUAL_UI(root->uid, sb.st_uid);
	TEST_EQUAL_UI(root->gid, sb.st_gid);
	TEST_EQUAL_UI(root->mode, sb.st_mode);
	TEST_EQUAL_UI(root->link_count, 2);
	TEST_ASSERT(root->name >= (char *)root->payload);
	TEST_STR_EQUAL(root->name, "rootdir");
	TEST_NULL(root->data.children);
	TEST_NULL(root->next);

	a = fstree_add_generic(&fs, "rootdir/adir", &sb, NULL);
	TEST_NOT_NULL(a);
	TEST_ASSERT(a->parent == root);
	TEST_NULL(a->next);
	TEST_EQUAL_UI(a->link_count, 2);
	TEST_EQUAL_UI(root->link_count, 3);
	TEST_ASSERT(root->data.children == a);
	TEST_ASSERT(root->parent == fs.root);
	TEST_NULL(root->next);

	b = fstree_add_generic(&fs, "rootdir/bdir", &sb, NULL);
	TEST_NOT_NULL(b);
	TEST_ASSERT(a->parent == root);
	TEST_ASSERT(b->parent == root);
	TEST_EQUAL_UI(b->link_count, 2);
	TEST_ASSERT(root->data.children == a);
	TEST_ASSERT(a->next == b);
	TEST_EQUAL_UI(root->link_count, 4);
	TEST_NULL(b->next);
	TEST_ASSERT(root->parent == fs.root);
	TEST_NULL(root->next);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
