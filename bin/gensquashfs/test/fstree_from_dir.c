/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "mkfs.h"

static void check_hierarchy(tree_node_t *root, bool recursive)
{
	tree_node_t *n, *m;

	n = root->data.dir.children;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "dira");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_a0");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_a1");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_a2");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "dirb");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_b0");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_b1");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_b2");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "dirc");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_c0");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_c1");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "file_c2");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NULL(n);
}

int main(int argc, char **argv)
{
	struct stat sb;
	tree_node_t *n;
	fstree_t fs;
	(void)argc; (void)argv;

	/* recursively scan into root */
	TEST_ASSERT(fstree_init(&fs, NULL) == 0);
	TEST_ASSERT(fstree_from_dir(&fs, fs.root, TEST_PATH,
				    NULL, NULL, 0) == 0);

	fstree_post_process(&fs);
	check_hierarchy(fs.root, true);
	fstree_cleanup(&fs);

	/* non-recursively scan into root */
	TEST_ASSERT(fstree_init(&fs, NULL) == 0);
	TEST_ASSERT(fstree_from_dir(&fs, fs.root, TEST_PATH, NULL, NULL,
				    DIR_SCAN_NO_RECURSION) == 0);

	fstree_post_process(&fs);
	check_hierarchy(fs.root, false);
	fstree_cleanup(&fs);

	/* recursively scan into a sub-directory of root */
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;

	TEST_ASSERT(fstree_init(&fs, NULL) == 0);

	n = fstree_mknode(fs.root, "foodir", 6, NULL, &sb);
	TEST_NOT_NULL(n);
	fs.root->data.dir.children = n;

	TEST_ASSERT(fstree_from_dir(&fs, n, TEST_PATH, NULL, NULL, 0) == 0);

	TEST_ASSERT(fs.root->data.dir.children == n);
	TEST_NULL(n->next);

	fstree_post_process(&fs);
	check_hierarchy(n, true);
	fstree_cleanup(&fs);

	/* non-recursively scan into a sub-directory of root */
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;

	TEST_ASSERT(fstree_init(&fs, NULL) == 0);

	n = fstree_mknode(fs.root, "foodir", 6, NULL, &sb);
	TEST_NOT_NULL(n);
	fs.root->data.dir.children = n;

	TEST_ASSERT(fstree_from_dir(&fs, n, TEST_PATH, NULL, NULL,
				    DIR_SCAN_NO_RECURSION) == 0);

	TEST_ASSERT(fs.root->data.dir.children == n);
	TEST_NULL(n->next);

	fstree_post_process(&fs);
	check_hierarchy(n, false);
	fstree_cleanup(&fs);

	return EXIT_SUCCESS;
}
