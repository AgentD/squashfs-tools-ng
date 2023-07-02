/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util/test.h"

static sqfs_dir_entry_t *mkentry(const char *name)
{
	sqfs_dir_entry_t *ent = sqfs_dir_entry_create(name, S_IFDIR | 0654, 0);
	TEST_NOT_NULL(ent);
	ent->uid = 123;
	ent->gid = 456;
	ent->rdev = 789;
	ent->size = 4096;
	return ent;
}

int main(int argc, char **argv)
{
	fstree_defaults_t defaults;
	tree_node_t *root, *a, *b;
	sqfs_dir_entry_t *ent;
	fstree_t fs;
	int ret;
	(void)argc; (void)argv;

	memset(&defaults, 0, sizeof(defaults));
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("rootdir");
	root = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(root);
	TEST_ASSERT(root->parent == fs.root);
	TEST_EQUAL_UI(root->uid, 123);
	TEST_EQUAL_UI(root->gid, 456);
	TEST_EQUAL_UI(root->mode, (S_IFDIR | 0654));
	TEST_EQUAL_UI(root->link_count, 2);
	TEST_ASSERT(root->name >= (char *)root->payload);
	TEST_STR_EQUAL(root->name, "rootdir");
	TEST_NULL(root->data.children);
	TEST_NULL(root->next);

	ent = mkentry("rootdir/adir");
	a = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(a);
	TEST_ASSERT(a->parent == root);
	TEST_NULL(a->next);
	TEST_EQUAL_UI(a->link_count, 2);
	TEST_EQUAL_UI(root->link_count, 3);
	TEST_ASSERT(root->data.children == a);
	TEST_ASSERT(root->parent == fs.root);
	TEST_NULL(root->next);

	ent = mkentry("rootdir/bdir");
	b = fstree_add_generic(&fs, ent, NULL);
	free(ent);
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
