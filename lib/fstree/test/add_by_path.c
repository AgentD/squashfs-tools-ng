/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * add_by_path.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "common.h"
#include "util/test.h"

static dir_entry_t *mkentry(const char *name, sqfs_u16 mode, sqfs_u32 uid,
			    sqfs_u32 gid)
{
	dir_entry_t *ent = dir_entry_create(name);

	TEST_NOT_NULL(ent);
	ent->mode = mode;
	ent->uid = uid;
	ent->gid = gid;
	return ent;
}

int main(int argc, char **argv)
{
	fstree_defaults_t fsd;
	tree_node_t *a, *b;
	dir_entry_t *ent;
	fstree_t fs;
	char *opts;
	(void)argc; (void)argv;

	opts = strdup("mode=0755,uid=21,gid=42");
	TEST_ASSERT(parse_fstree_defaults(&fsd, opts) == 0);
	free(opts);
	TEST_ASSERT(fstree_init(&fs, &fsd) == 0);
	TEST_EQUAL_UI(fs.root->link_count, 2);

	ent = mkentry("dir", S_IFDIR | 0750, 1000, 100);
	a = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(a);
	TEST_STR_EQUAL(a->name, "dir");
	TEST_EQUAL_UI(a->mode, (S_IFDIR | 0750));
	TEST_EQUAL_UI(a->uid, 1000);
	TEST_EQUAL_UI(a->gid, 100);
	TEST_ASSERT(a->parent == fs.root);
	TEST_EQUAL_UI(a->link_count, 2);
	TEST_NULL(a->next);
	TEST_ASSERT(fs.root->data.children == a);
	TEST_EQUAL_UI(fs.root->link_count, 3);
	TEST_ASSERT(!(a->flags & FLAG_DIR_CREATED_IMPLICITLY));

	ent = mkentry("blkdev", S_IFBLK | 0640, 0, 0);
	ent->rdev = 1234;
	b = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(b);
	TEST_ASSERT(b != a);
	TEST_STR_EQUAL(b->name, "blkdev");
	TEST_EQUAL_UI(b->mode, (S_IFBLK | 0640));
	TEST_EQUAL_UI(b->uid, 0);
	TEST_EQUAL_UI(b->gid, 0);
	TEST_ASSERT(b->parent == fs.root);
	TEST_EQUAL_UI(b->link_count, 1);
	TEST_EQUAL_UI(b->data.devno, 1234);
	TEST_ASSERT(b->next == a);
	TEST_EQUAL_UI(fs.root->link_count, 4);
	TEST_ASSERT(fs.root->data.children == b);

	ent = mkentry("blkdev/foo", S_IFBLK | 0640, 0, 0);
	TEST_NULL(fstree_add_generic(&fs, ent, NULL));
	TEST_EQUAL_UI(errno, ENOTDIR);
	free(ent);

	ent = mkentry("dir", S_IFBLK | 0640, 0, 0);
	TEST_NULL(fstree_add_generic(&fs, ent, NULL));
	TEST_EQUAL_UI(errno, EEXIST);
	free(ent);

	ent = mkentry("dir", S_IFDIR | 0755, 0, 0);
	TEST_NULL(fstree_add_generic(&fs, ent, NULL));
	TEST_EQUAL_UI(errno, EEXIST);
	free(ent);

	ent = mkentry("dir/chrdev", S_IFCHR | 0444, 0, 0);
	ent->rdev = 5678;
	b = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(b);
	TEST_EQUAL_UI(b->mode, (S_IFCHR | 0444));
	TEST_EQUAL_UI(b->uid, 0);
	TEST_EQUAL_UI(b->gid, 0);
	TEST_EQUAL_UI(b->link_count, 1);
	TEST_ASSERT(b->parent == a);
	TEST_EQUAL_UI(b->data.devno, 5678);
	TEST_NULL(b->next);
	TEST_ASSERT(a->data.children == b);

	TEST_EQUAL_UI(a->link_count, 3);
	TEST_EQUAL_UI(fs.root->link_count, 4);

	ent = mkentry("dir/foo/chrdev", S_IFCHR | 0444, 0, 0);
	ent->rdev = 91011;
	b = fstree_add_generic(&fs, ent, NULL);
	free(ent);
	TEST_NOT_NULL(b);
	TEST_NULL(b->next);
	TEST_EQUAL_UI(b->mode, (S_IFCHR | 0444));
	TEST_EQUAL_UI(b->uid, 0);
	TEST_EQUAL_UI(b->gid, 0);
	TEST_EQUAL_UI(b->link_count, 1);
	TEST_ASSERT(b->parent != a);
	TEST_ASSERT(b->parent->parent == a);
	TEST_EQUAL_UI(b->data.devno, 91011);
	TEST_NULL(b->next);

	TEST_EQUAL_UI(a->link_count, 4);
	TEST_EQUAL_UI(fs.root->link_count, 4);
	TEST_ASSERT(a->data.children != b);

	b = b->parent;
	TEST_ASSERT((b->flags & FLAG_DIR_CREATED_IMPLICITLY));
	TEST_EQUAL_UI(b->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(b->uid, 21);
	TEST_EQUAL_UI(b->gid, 42);
	TEST_EQUAL_UI(b->link_count, 3);

	ent = mkentry("dir/foo", S_IFDIR | 0755, 1000, 100);
	a = fstree_add_generic(&fs, ent, NULL);
	TEST_NOT_NULL(a);
	TEST_ASSERT(a == b);
	TEST_ASSERT(!(a->flags & FLAG_DIR_CREATED_IMPLICITLY));
	TEST_EQUAL_UI(a->mode, (S_IFDIR | 0755));
	TEST_EQUAL_UI(a->uid, 1000);
	TEST_EQUAL_UI(a->gid, 100);
	TEST_EQUAL_UI(a->link_count, 3);

	TEST_EQUAL_UI(a->parent->link_count, 4);
	TEST_EQUAL_UI(fs.root->link_count, 4);

	TEST_NULL(fstree_add_generic(&fs, ent, NULL));
	TEST_EQUAL_UI(errno, EEXIST);
	free(ent);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
