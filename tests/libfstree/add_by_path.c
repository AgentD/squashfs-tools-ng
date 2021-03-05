/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * add_by_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "../test.h"

int main(void)
{
	tree_node_t *a, *b;
	struct stat sb;
	fstree_t fs;
	char *opts;

	opts = strdup("mode=0755,uid=21,gid=42");
	TEST_ASSERT(fstree_init(&fs, opts) == 0);
	free(opts);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0750;
	sb.st_uid = 1000;
	sb.st_gid = 100;

	TEST_EQUAL_UI(fs.root->link_count, 2);

	a = fstree_add_generic(&fs, "dir", &sb, NULL);
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "dir");
	TEST_EQUAL_UI(a->mode, sb.st_mode);
	TEST_EQUAL_UI(a->uid, sb.st_uid);
	TEST_EQUAL_UI(a->gid, sb.st_gid);
	TEST_ASSERT(a->parent == fs.root);
	TEST_EQUAL_UI(a->link_count, 2);
	TEST_NULL(a->next);
	TEST_ASSERT(fs.root->data.dir.children == a);
	TEST_EQUAL_UI(fs.root->link_count, 3);
	TEST_ASSERT(!a->data.dir.created_implicitly);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0640;
	sb.st_rdev = 1234;

	b = fstree_add_generic(&fs, "blkdev", &sb, NULL);
	TEST_NOT_NULL(b);
	TEST_ASSERT(b != a);
	TEST_STR_EQUAL(b->name, "blkdev");
	TEST_EQUAL_UI(b->mode, sb.st_mode);
	TEST_EQUAL_UI(b->uid, sb.st_uid);
	TEST_EQUAL_UI(b->gid, sb.st_gid);
	TEST_ASSERT(b->parent == fs.root);
	TEST_EQUAL_UI(b->link_count, 1);
	TEST_EQUAL_UI(b->data.devno, sb.st_rdev);
	TEST_ASSERT(b->next == a);
	TEST_EQUAL_UI(fs.root->link_count, 4);
	TEST_ASSERT(fs.root->data.dir.children == b);

	TEST_NULL(fstree_add_generic(&fs, "blkdev/foo", &sb, NULL));
	TEST_EQUAL_UI(errno, ENOTDIR);

	TEST_NULL(fstree_add_generic(&fs, "dir", &sb, NULL));
	TEST_EQUAL_UI(errno, EEXIST);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;
	TEST_NULL(fstree_add_generic(&fs, "dir", &sb, NULL));
	TEST_EQUAL_UI(errno, EEXIST);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFCHR | 0444;
	b = fstree_add_generic(&fs, "dir/chrdev", &sb, NULL);
	TEST_NOT_NULL(b);
	TEST_EQUAL_UI(b->mode, sb.st_mode);
	TEST_EQUAL_UI(b->uid, sb.st_uid);
	TEST_EQUAL_UI(b->gid, sb.st_gid);
	TEST_EQUAL_UI(b->link_count, 1);
	TEST_ASSERT(b->parent == a);
	TEST_EQUAL_UI(b->data.devno, sb.st_rdev);
	TEST_NULL(b->next);
	TEST_ASSERT(a->data.dir.children == b);

	TEST_EQUAL_UI(a->link_count, 3);
	TEST_EQUAL_UI(fs.root->link_count, 4);

	b = fstree_add_generic(&fs, "dir/foo/chrdev", &sb, NULL);
	TEST_NOT_NULL(b);
	TEST_NULL(b->next);
	TEST_EQUAL_UI(b->mode, sb.st_mode);
	TEST_EQUAL_UI(b->uid, sb.st_uid);
	TEST_EQUAL_UI(b->gid, sb.st_gid);
	TEST_EQUAL_UI(b->link_count, 1);
	TEST_ASSERT(b->parent != a);
	TEST_ASSERT(b->parent->parent == a);
	TEST_EQUAL_UI(b->data.devno, sb.st_rdev);
	TEST_NULL(b->next);

	TEST_EQUAL_UI(a->link_count, 4);
	TEST_EQUAL_UI(fs.root->link_count, 4);
	TEST_ASSERT(a->data.dir.children != b);

	b = b->parent;
	TEST_ASSERT(b->data.dir.created_implicitly);
	TEST_EQUAL_UI(b->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(b->uid, 21);
	TEST_EQUAL_UI(b->gid, 42);
	TEST_EQUAL_UI(b->link_count, 3);

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0750;
	sb.st_uid = 1000;
	sb.st_gid = 100;

	a = fstree_add_generic(&fs, "dir/foo", &sb, NULL);
	TEST_NOT_NULL(a);
	TEST_ASSERT(a == b);
	TEST_ASSERT(!a->data.dir.created_implicitly);
	TEST_EQUAL_UI(a->mode, sb.st_mode);
	TEST_EQUAL_UI(a->uid, sb.st_uid);
	TEST_EQUAL_UI(a->gid, sb.st_gid);
	TEST_EQUAL_UI(a->link_count, 3);

	TEST_EQUAL_UI(a->parent->link_count, 4);
	TEST_EQUAL_UI(fs.root->link_count, 4);

	TEST_NULL(fstree_add_generic(&fs, "dir/foo", &sb, NULL));
	TEST_EQUAL_UI(errno, EEXIST);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
