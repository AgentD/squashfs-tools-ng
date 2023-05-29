/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_simple.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "util/test.h"

static dir_entry_t *mkentry(const char *name, sqfs_u16 mode)
{
	dir_entry_t *ent = calloc(1, sizeof(*ent) + strlen(name) + 1);

	TEST_NOT_NULL(ent);
	strcpy(ent->name, name);
	ent->mode = mode | 0654;
	ent->uid = 123;
	ent->gid = 456;
	ent->rdev = 789;
	ent->size = 1337;
	return ent;
}

int main(int argc, char **argv)
{
	fstree_defaults_t defaults;
	tree_node_t *node;
	dir_entry_t *ent;
	fstree_t fs;
	int ret;
	(void)argc; (void)argv;

	memset(&defaults, 0, sizeof(defaults));
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("/sockfile", S_IFSOCK);
	node = fstree_add_generic(&fs, ent, "target");
	free(ent);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "sockfile");
	TEST_EQUAL_UI(node->uid, 123);
	TEST_EQUAL_UI(node->gid, 456);
	TEST_EQUAL_UI(node->mode, (S_IFSOCK | 0654));
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->data.target);
	TEST_EQUAL_UI(node->data.devno, 0);

	fstree_cleanup(&fs);
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("/fifo", S_IFIFO);
	node = fstree_add_generic(&fs, ent, "target");
	free(ent);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "fifo");
	TEST_EQUAL_UI(node->uid, 123);
	TEST_EQUAL_UI(node->gid, 456);
	TEST_EQUAL_UI(node->mode, (S_IFIFO | 0654));
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_NULL(node->data.target);
	TEST_EQUAL_UI(node->data.devno, 0);

	fstree_cleanup(&fs);
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("/blkdev", S_IFBLK);
	node = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "blkdev");
	TEST_EQUAL_UI(node->uid, 123);
	TEST_EQUAL_UI(node->gid, 456);
	TEST_EQUAL_UI(node->mode, (S_IFBLK | 0654));
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_EQUAL_UI(node->data.devno, 789);

	fstree_cleanup(&fs);
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("/chardev", S_IFCHR);
	node = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_STR_EQUAL(node->name, "chardev");
	TEST_EQUAL_UI(node->uid, 123);
	TEST_EQUAL_UI(node->gid, 456);
	TEST_EQUAL_UI(node->mode, (S_IFCHR | 0654));
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_EQUAL_UI(node->data.devno, 789);
	fstree_cleanup(&fs);

	memset(&defaults, 0, sizeof(defaults));
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("/symlink", S_IFLNK);
	node = fstree_add_generic(&fs, ent, "target");
	free(ent);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_EQUAL_UI(node->uid, 123);
	TEST_EQUAL_UI(node->gid, 456);
	TEST_EQUAL_UI(node->mode, (S_IFLNK | 0777));
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= node->name + 8);
	TEST_STR_EQUAL(node->name, "symlink");
	TEST_STR_EQUAL(node->data.target, "target");
	fstree_cleanup(&fs);

	memset(&defaults, 0, sizeof(defaults));
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("/foo", S_IFLNK);
	node = fstree_add_generic(&fs, ent, "");
	free(ent);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_EQUAL_UI(node->uid, 123);
	TEST_EQUAL_UI(node->gid, 456);
	TEST_EQUAL_UI(node->mode, (S_IFLNK | 0777));
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= (char *)node->payload);
	TEST_ASSERT(node->data.target >= node->name + 3);
	TEST_STR_EQUAL(node->name, "foo");
	TEST_STR_EQUAL(node->data.target, "");
	fstree_cleanup(&fs);

	memset(&defaults, 0, sizeof(defaults));
	ret = fstree_init(&fs, &defaults);
	TEST_EQUAL_I(ret, 0);

	ent = mkentry("/filename", S_IFREG);
	node = fstree_add_generic(&fs, ent, "input");
	free(ent);
	TEST_NOT_NULL(node);
	TEST_ASSERT(node->parent == fs.root);
	TEST_EQUAL_UI(node->uid, 123);
	TEST_EQUAL_UI(node->gid, 456);
	TEST_EQUAL_UI(node->mode, (S_IFREG | 0654));
	TEST_EQUAL_UI(node->link_count, 1);
	TEST_ASSERT((char *)node->name >= (char *)node->payload);
	TEST_ASSERT(node->data.file.input_file >= (char *)node->payload);
	TEST_ASSERT(node->data.file.input_file >= node->name + 8);
	TEST_STR_EQUAL(node->name, "filename");
	TEST_STR_EQUAL(node->data.file.input_file, "input");
	fstree_cleanup(&fs);

	return EXIT_SUCCESS;
}
