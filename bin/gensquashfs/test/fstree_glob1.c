/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_glob1.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "mkfs.h"

static void check_hierarchy(tree_node_t *root, bool subdir, bool recursive)
{
	tree_node_t *n, *m, *parentdir;

	if (subdir) {
		n = root->data.children;
		TEST_NOT_NULL(n);
		TEST_STR_EQUAL(n->name, "tarcorpus");
		TEST_ASSERT(S_ISDIR(n->mode));
		TEST_ASSERT(n->parent == root);
		TEST_NULL(n->next);
	} else {
		n = root;
		TEST_NOT_NULL(n);
		TEST_STR_EQUAL(n->name, "");
		TEST_ASSERT(S_ISDIR(n->mode));
		TEST_NULL(n->parent);
		TEST_NULL(n->next);
	}

	parentdir = n;
	n = n->data.children;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "file-size");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);

	if (recursive) {
		m = n->data.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
			       "../../../lib/tar/test/data/file-size/gnu.tar");

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "format-acceptance");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);

	if (recursive) {
		m = n->data.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu-g.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/format-acceptance/gnu-g.tar");

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/format-acceptance/gnu.tar");

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "iterator");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);
	TEST_NULL(n->data.children);

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "large-mtime");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);

	if (recursive) {
		m = n->data.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
			     "../../../lib/tar/test/data/large-mtime/gnu.tar");

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "long-paths");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);

	if (recursive) {
		m = n->data.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
			     "../../../lib/tar/test/data/long-paths/gnu.tar");

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "negative-mtime");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);

	if (recursive) {
		m = n->data.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
			   "../../../lib/tar/test/data/negative-mtime/gnu.tar");

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "sparse-files");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);

	if (recursive) {
		m = n->data.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu-small.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/sparse-files/gnu-small.tar");

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/sparse-files/gnu.tar");

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax-gnu0-0.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/sparse-files/pax-gnu0-0.tar");

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax-gnu0-1.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/sparse-files/pax-gnu0-1.tar");

		m = m->next;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "pax-gnu1-0.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/sparse-files/pax-gnu1-0.tar");

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "user-group-largenum");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);

	if (recursive) {
		m = n->data.children;
		TEST_NOT_NULL(m);
		TEST_STR_EQUAL(m->name, "gnu.tar");
		TEST_ASSERT(S_ISREG(m->mode));
		TEST_ASSERT(m->parent == n);

		TEST_NOT_NULL(m->data.file.input_file);
		TEST_STR_EQUAL(m->data.file.input_file,
		"../../../lib/tar/test/data/user-group-largenum/gnu.tar");

		m = m->next;
		TEST_NULL(m);
	} else {
		TEST_NULL(n->data.children);
	}

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "write");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);
	TEST_NULL(n->data.children);

	n = n->next;
	TEST_NOT_NULL(n);
	TEST_STR_EQUAL(n->name, "xattr");
	TEST_ASSERT(S_ISDIR(n->mode));
	TEST_ASSERT(n->parent == parentdir);
	TEST_NULL(n->data.children);

	n = n->next;
	TEST_NULL(n);
}

int main(int argc, char **argv)
{
	fstree_defaults_t fsd;
	options_t opt;
	fstree_t fs;
	int ret;
	(void)argc; (void)argv;

	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);

	memset(&opt, 0, sizeof(opt));
	opt.packdir = (char *)TEST_PATH;

	/* first test case, directory tree only */
	ret = fstree_init(&fs, &fsd);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_from_file(&fs, TEST_PATH "/fstree_glob1.txt", &opt);
	TEST_EQUAL_I(ret, 0);

	fstree_post_process(&fs);
	check_hierarchy(fs.root, true, false);
	fstree_cleanup(&fs);

	/* first test case, directory tree plus fnmatch()ed files */
	ret = fstree_init(&fs, &fsd);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_from_file(&fs, TEST_PATH "/fstree_glob2.txt", &opt);
	TEST_EQUAL_I(ret, 0);

	fstree_post_process(&fs);
	check_hierarchy(fs.root, true, true);
	fstree_cleanup(&fs);

	/* third test case, same as second, but entries directly at the root */
	ret = fstree_init(&fs, &fsd);
	TEST_EQUAL_I(ret, 0);

	ret = fstree_from_file(&fs, TEST_PATH "/fstree_glob3.txt", &opt);
	TEST_EQUAL_I(ret, 0);

	fstree_post_process(&fs);
	check_hierarchy(fs.root, false, true);
	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
