/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * gen_inode_table.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

static tree_node_t *gen_node(tree_node_t *parent, const char *name)
{
	struct stat sb;

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;

	return fstree_mknode(parent, name, strlen(name), NULL, &sb);
}

static void check_children_before_root(tree_node_t *root)
{
	tree_node_t *n;

	for (n = root->data.dir.children; n != NULL; n = n->next)
		assert(n->inode_num < root->inode_num);

	for (n = root->data.dir.children; n != NULL; n = n->next)
		check_children_before_root(n);
}

static void check_children_continuous(tree_node_t *root)
{
	tree_node_t *n;

	for (n = root->data.dir.children; n != NULL; n = n->next) {
		if (n->next != NULL) {
			assert(n->next->inode_num == (n->inode_num + 1));
		}
	}

	for (n = root->data.dir.children; n != NULL; n = n->next)
		check_children_continuous(n);
}

int main(void)
{
	tree_node_t *a, *b, *c;
	fstree_t fs;

	// inode table for the empty tree
	assert(fstree_init(&fs, NULL) == 0);
	fstree_gen_inode_numbers(&fs);
	assert(fs.unique_inode_count == 1);
	assert(fs.root->inode_num == 1);
	fstree_cleanup(&fs);

	// tree with 2 levels under root, fan out 3
	assert(fstree_init(&fs, NULL) == 0);

	a = gen_node(fs.root, "a");
	b = gen_node(fs.root, "b");
	c = gen_node(fs.root, "c");
	assert(a != NULL);
	assert(b != NULL);
	assert(c != NULL);

	assert(gen_node(a, "a_a") != NULL);
	assert(gen_node(a, "a_b") != NULL);
	assert(gen_node(a, "a_c") != NULL);

	assert(gen_node(b, "b_a") != NULL);
	assert(gen_node(b, "b_b") != NULL);
	assert(gen_node(b, "b_c") != NULL);

	assert(gen_node(c, "c_a") != NULL);
	assert(gen_node(c, "c_b") != NULL);
	assert(gen_node(c, "c_c") != NULL);

	fstree_gen_inode_numbers(&fs);
	assert(fs.unique_inode_count == 13);

	check_children_before_root(fs.root);
	check_children_continuous(fs.root);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
