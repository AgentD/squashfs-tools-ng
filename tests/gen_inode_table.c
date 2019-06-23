/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

static tree_node_t *gen_node(fstree_t *fs, tree_node_t *parent,
			     const char *name)
{
	struct stat sb;

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0755;

	return fstree_mknode(fs, parent, name, strlen(name), NULL, &sb);
}

static void check_children_before_root(tree_node_t *root)
{
	tree_node_t *n;

	for (n = root->data.dir->children; n != NULL; n = n->next)
		assert(n->inode_num < root->inode_num);

	for (n = root->data.dir->children; n != NULL; n = n->next)
		check_children_before_root(n);
}

static void check_children_continuous(tree_node_t *root)
{
	tree_node_t *n;

	for (n = root->data.dir->children; n != NULL; n = n->next) {
		if (n->next != NULL) {
			assert(n->next->inode_num == (n->inode_num + 1));
		}
	}

	for (n = root->data.dir->children; n != NULL; n = n->next)
		check_children_continuous(n);
}

int main(void)
{
	tree_node_t *a, *b, *c;
	unsigned int i;
	fstree_t fs;

	// inode table for the empty tree
	assert(fstree_init(&fs, 0, NULL) == 0);
	assert(fstree_gen_inode_table(&fs) == 0);
	assert(fs.inode_tbl_size == 3);
	assert(fs.root->inode_num == 2);
	assert(fs.inode_table[0] == NULL);
	assert(fs.inode_table[1] == NULL);
	assert(fs.inode_table[2] == fs.root);
	fstree_cleanup(&fs);

	// tree with 2 levels under root, fan out 3
	assert(fstree_init(&fs, 0, NULL) == 0);

	a = gen_node(&fs, fs.root, "a");
	b = gen_node(&fs, fs.root, "b");
	c = gen_node(&fs, fs.root, "c");
	assert(a != NULL);
	assert(b != NULL);
	assert(c != NULL);

	assert(gen_node(&fs, a, "a_a") != NULL);
	assert(gen_node(&fs, a, "a_b") != NULL);
	assert(gen_node(&fs, a, "a_c") != NULL);

	assert(gen_node(&fs, b, "b_a") != NULL);
	assert(gen_node(&fs, b, "b_b") != NULL);
	assert(gen_node(&fs, b, "b_c") != NULL);

	assert(gen_node(&fs, c, "c_a") != NULL);
	assert(gen_node(&fs, c, "c_b") != NULL);
	assert(gen_node(&fs, c, "c_c") != NULL);

	assert(fstree_gen_inode_table(&fs) == 0);

	assert(fs.inode_tbl_size == (13 + 2));
	assert(fs.inode_table[0] == NULL);
	assert(fs.inode_table[1] == NULL);

	for (i = 2; i < (13 + 2); ++i) {
		assert(fs.inode_table[i] != NULL);
		assert(fs.inode_table[i]->inode_num == i);
	}

	check_children_before_root(fs.root);
	check_children_continuous(fs.root);

	fstree_cleanup(&fs);
	return EXIT_SUCCESS;
}
