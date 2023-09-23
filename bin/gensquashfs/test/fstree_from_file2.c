/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file2.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/test.h"
#include "mkfs.h"

const char *listing =
"dir /test 0755 0 0\n"
"file /test/file1 0644 0 0\n"
"file /test/file2 0644 0 0 completely/different/path\n"
"file /test/file3 0644 0 0 /absolute/path\n"
"file /test/file4 0644 0 0 \"/ \x21 \"\n";

int main(int argc, char **argv)
{
	fstree_defaults_t fsd;
	sqfs_istream_t *file;
	tree_node_t *n;
	options_t opt;
	fstree_t fs;
	(void)argc; (void)argv;

	memset(&opt, 0, sizeof(opt));

	file = istream_memory_create("memfile", 7, listing, strlen(listing));
	TEST_NOT_NULL(file);

	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);
	TEST_ASSERT(fstree_init(&fs, &fsd) == 0);
	TEST_ASSERT(fstree_from_file_stream(&fs, file, &opt) == 0);
	sqfs_drop(file);

	fstree_post_process(&fs);
	TEST_EQUAL_UI(fs.root->link_count, 3);
	TEST_EQUAL_UI(fs.root->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(fs.root->uid, 0);
	TEST_EQUAL_UI(fs.root->gid, 0);

	n = fs.root->data.children;
	TEST_NOT_NULL(n);

	TEST_EQUAL_UI(n->mode, S_IFDIR | 0755);
	TEST_EQUAL_UI(n->uid, 0);
	TEST_EQUAL_UI(n->gid, 0);
	TEST_EQUAL_UI(n->link_count, 6);
	TEST_STR_EQUAL(n->name, "test");
	TEST_NULL(n->next);

	n = n->data.children;
	TEST_NOT_NULL(n);

	TEST_EQUAL_UI(n->mode, S_IFREG | 0644);
	TEST_EQUAL_UI(n->uid, 0);
	TEST_EQUAL_UI(n->gid, 0);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "file1");
	TEST_NOT_NULL(n->data.file.input_file);
	TEST_STR_EQUAL(n->data.file.input_file, "test/file1");
	TEST_NOT_NULL(n->next);
	n = n->next;

	TEST_EQUAL_UI(n->mode, S_IFREG | 0644);
	TEST_EQUAL_UI(n->uid, 0);
	TEST_EQUAL_UI(n->gid, 0);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "file2");
	TEST_NOT_NULL(n->data.file.input_file);
	TEST_STR_EQUAL(n->data.file.input_file, "completely/different/path");
	TEST_NOT_NULL(n->next);
	n = n->next;

	TEST_EQUAL_UI(n->mode, S_IFREG | 0644);
	TEST_EQUAL_UI(n->uid, 0);
	TEST_EQUAL_UI(n->gid, 0);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "file3");
	TEST_NOT_NULL(n->data.file.input_file);
	TEST_STR_EQUAL(n->data.file.input_file, "/absolute/path");
	TEST_NOT_NULL(n->next);
	n = n->next;

	TEST_EQUAL_UI(n->mode, S_IFREG | 0644);
	TEST_EQUAL_UI(n->uid, 0);
	TEST_EQUAL_UI(n->gid, 0);
	TEST_EQUAL_UI(n->link_count, 1);
	TEST_STR_EQUAL(n->name, "file4");
	TEST_NOT_NULL(n->data.file.input_file);
	TEST_STR_EQUAL(n->data.file.input_file, "/ ! ");
	TEST_NULL(n->next);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
