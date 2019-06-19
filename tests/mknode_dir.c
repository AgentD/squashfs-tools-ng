/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(void)
{
	tree_node_t *root, *a, *b;
	struct stat sb;
	fstree_t fs;

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFDIR | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 4096;

	root = fstree_mknode(&fs, NULL, "rootdir", 7, (void *)0x100, &sb);
	assert(root->uid == sb.st_uid);
	assert(root->gid == sb.st_gid);
	assert(root->mode == sb.st_mode);
	assert((char *)root->name >= (char *)root->payload);
	assert((char *)root->data.dir >= (char *)root->payload);
	assert(root->name >= (char *)(root->data.dir + 1));
	assert(strcmp(root->name, "rootdir") == 0);
	assert(root->data.dir->children == NULL);
	assert(root->parent == NULL);
	assert(root->next == NULL);

	a = fstree_mknode(&fs, root, "adir", 4, (void *)0x100, &sb);
	assert(a->parent == root);
	assert(a->next == NULL);
	assert(root->data.dir->children == a);
	assert(root->parent == NULL);
	assert(root->next == NULL);

	b = fstree_mknode(&fs, root, "bdir", 4, (void *)0x100, &sb);
	assert(a->parent == root);
	assert(b->parent == root);
	assert(root->data.dir->children == b);
	assert(b->next == a);
	assert(a->next == NULL);
	assert(root->parent == NULL);
	assert(root->next == NULL);

	free(root);
	free(a);
	free(b);

	return EXIT_SUCCESS;
}
