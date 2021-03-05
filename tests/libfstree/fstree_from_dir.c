/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "../test.h"

static void check_hierarchy(tree_node_t *root, bool recursive)
{
	tree_node_t *n, *m;

	n = root->data.dir.children;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "CREDITS");
	TEST_ASSERT(S_ISREG(n->mode));
	TEST_ASSERT(n->parent == root);

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "file-size");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "12-digit.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "format-acceptance");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu-g.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "link_filled.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "ustar-pre-posix.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "ustar.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "v7.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "large-mtime");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "12-digit.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "long-paths");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "ustar.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "negative-mtime");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "sparse-files");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu-small.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax-gnu0-0.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax-gnu0-1.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax-gnu1-0.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "sqfs.sha512");
	TEST_ASSERT(S_ISREG(n->mode));
	TEST_ASSERT(n->parent == root);

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "user-group-largenum");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "8-digit.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.dir.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "xattr");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == root);

	if (recursive) {
		m = n->data.dir.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "acl.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "xattr-libarchive.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "xattr-schily-binary.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "xattr-schily.tar");
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

int main(void)
{
	struct stat sb;
	tree_node_t *n;
	fstree_t fs;

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
