/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "../test.h"

int main(void)
{
	tree_node_t *n;
	fstree_t fs;

	TEST_ASSERT(fstree_init(&fs, NULL) == 0);
	TEST_ASSERT(fstree_from_file(&fs, TEST_PATH, NULL) == 0);

	fstree_post_process(&fs);
	n = fs.root->data.dir.children;

	TEST_EQUAL_UI(fs.root->link_count, 9);
	TEST_EQUAL_UI(fs.root->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(fs.root->uid, 1000);
	TEST_EQUAL_UI(fs.root->gid, 100);

	TEST_EQUAL_UI(n->mode, S_IFBLK | 0600);
	TEST_EQUAL_UI(n->uid, 8);
	TEST_EQUAL_UI(n->gid, 9);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "blkdev");
	TEST_EQUAL_UI(n->data.devno, makedev(42, 21));

	n = n->next;
	TEST_EQUAL_UI(n->mode, S_IFCHR | 0600);
	TEST_EQUAL_UI(n->uid, 6);
	TEST_EQUAL_UI(n->gid, 7);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "chardev");
	TEST_EQUAL_UI(n->data.devno, makedev(13, 37));

	n = n->next;
	TEST_EQUAL_UI(n->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(n->uid, 4);
	TEST_EQUAL_UI(n->gid, 5);
	TEST_EQUAL_UI(n->link_count, 2);
	TEST_STR_EQUAL(n->name, "dir");
	TEST_NULL(n->data.dir.children);

	n = n->next;
	TEST_EQUAL_UI(n->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(n->uid, 0);
	TEST_EQUAL_UI(n->gid, 0);
	TEST_EQUAL_UI(n->link_count, 3);
	TEST_STR_EQUAL(n->name, "foo bar");
	TEST_NOT_NULL(n->data.dir.children);

	TEST_NULL(n->data.dir.children->next);
	TEST_EQUAL_UI(n->data.dir.children->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(n->data.dir.children->uid, 0);
	TEST_EQUAL_UI(n->data.dir.children->gid, 0);
	TEST_EQUAL_UI(n->data.dir.children->link_count, 2);
	TEST_STR_EQUAL(n->data.dir.children->name, " test \"");
	TEST_NULL(n->data.dir.children->data.dir.children);

	n = n->next;
	TEST_EQUAL_UI(n->mode, S_IFIFO | 0644);
	TEST_EQUAL_UI(n->uid, 10);
	TEST_EQUAL_UI(n->gid, 11);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "pipe");

	n = n->next;
	TEST_EQUAL_UI(n->mode, S_IFLNK | 0777);
	TEST_EQUAL_UI(n->uid, 2);
	TEST_EQUAL_UI(n->gid, 3);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "slink");
	TEST_STR_EQUAL(n->data.target, "slinktarget");

	n = n->next;
	TEST_EQUAL_UI(n->mode, S_IFSOCK | 0555);
	TEST_EQUAL_UI(n->uid, 12);
	TEST_EQUAL_UI(n->gid, 13);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "sock");
	TEST_NULL(n->next);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
