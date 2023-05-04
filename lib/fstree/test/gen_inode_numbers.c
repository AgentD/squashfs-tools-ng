/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gen_inode_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "common.h"
#include "util/test.h"

static tree_node_t *gen_node(fstree_t *fs, const char *path)
{
	struct stat sb;

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;

	return fstree_add_generic(fs, path, &sb, NULL);
}

static void check_children_before_root(tree_node_t *root)
{
	tree_node_t *n;

	for (n = root->data.children; n != NULL; n = n->next)
		TEST_LESS_THAN_UI(n->inode_num, root->inode_num);

	for (n = root->data.children; n != NULL; n = n->next)
		check_children_before_root(n);
}

static void check_children_continuous(tree_node_t *root)
{
	tree_node_t *n;

	for (n = root->data.children; n != NULL; n = n->next) {
		if (n->next != NULL) {
			TEST_EQUAL_UI(n->next->inode_num, (n->inode_num + 1));
		}
	}

	for (n = root->data.children; n != NULL; n = n->next)
		check_children_continuous(n);
}

int main(int argc, char **argv)
{
	tree_node_t *a, *b, *c;
	fstree_defaults_t fsd;
	fstree_t fs;
	(void)argc; (void)argv;

	// inode table for the empty tree
	TEST_ASSERT(parse_fstree_defaults(&fsd, NULL) == 0);
	TEST_ASSERT(fstree_init(&fs, &fsd) == 0);
	fstree_post_process(&fs);
	TEST_EQUAL_UI(fs.unique_inode_count, 1);
	TEST_EQUAL_UI(fs.root->inode_num, 1);
	fstree_cleanup(&fs);

	// tree with 2 levels under root, fan out 3
	TEST_ASSERT(fstree_init(&fs, &fsd) == 0);

	a = gen_node(&fs, "a");
	b = gen_node(&fs, "b");
	c = gen_node(&fs, "c");
	TEST_NOT_NULL(a);
	TEST_NOT_NULL(b);
	TEST_NOT_NULL(c);

	TEST_NOT_NULL(gen_node(&fs, "a/a_a"));
	TEST_NOT_NULL(gen_node(&fs, "a/a_b"));
	TEST_NOT_NULL(gen_node(&fs, "a/a_c"));

	TEST_NOT_NULL(gen_node(&fs, "b/b_a"));
	TEST_NOT_NULL(gen_node(&fs, "b/b_b"));
	TEST_NOT_NULL(gen_node(&fs, "b/b_c"));

	TEST_NOT_NULL(gen_node(&fs, "c/c_a"));
	TEST_NOT_NULL(gen_node(&fs, "c/c_b"));
	TEST_NOT_NULL(gen_node(&fs, "c/c_c"));

	fstree_post_process(&fs);
	TEST_EQUAL_UI(fs.unique_inode_count, 13);

	check_children_before_root(fs.root);
	check_children_continuous(fs.root);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
