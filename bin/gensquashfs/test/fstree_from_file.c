/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "io/mem.h"
#include "mkfs.h"

const char *listing =
"# comment line\n"
"slink /slink 0644 2 3 slinktarget\n"
"dir /dir 0755 4 5\n"
"nod /chardev 0600 6 7 c 13 37\n"
"nod /blkdev 0600 8 9 b 42 21\n"
"pipe /pipe 0644 10 11\n"
"dir / 0755 1000 100\n"
"dir \"/foo bar\" 0755 0 0\n"
"dir \"/foo bar/ test \\\"/\" 0755 0 0\n"
"  sock  /sock  0555  12  13  ";

int main(int argc, char **argv)
{
	fstree_defaults_t fsd;
	istream_t *file;
	tree_node_t *n;
	fstree_t fs;
	(void)argc; (void)argv;

	file = istream_memory_create("memfile", 7, listing, strlen(listing));
	TEST_NOT_NULL(file);

	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);
	TEST_ASSERT(fstree_init(&fs, &fsd) == 0);
	TEST_ASSERT(fstree_from_file_stream(&fs, file, NULL) == 0);
	sqfs_drop(file);

	fstree_post_process(&fs);
	n = fs.root->data.children;

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
	TEST_NULL(n->data.children);

	n = n->next;
	TEST_EQUAL_UI(n->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(n->uid, 0);
	TEST_EQUAL_UI(n->gid, 0);
	TEST_EQUAL_UI(n->link_count, 3);
	TEST_STR_EQUAL(n->name, "foo bar");
	TEST_NOT_NULL(n->data.children);

	TEST_NULL(n->data.children->next);
	TEST_EQUAL_UI(n->data.children->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(n->data.children->uid, 0);
	TEST_EQUAL_UI(n->data.children->gid, 0);
	TEST_EQUAL_UI(n->data.children->link_count, 2);
	TEST_STR_EQUAL(n->data.children->name, " test \"");
	TEST_NULL(n->data.children->data.children);

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
