/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mknode_reg.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

int main(void)
{
	tree_node_t *node;
	struct stat sb;
	fstree_t fs;

	memset(&fs, 0, sizeof(fs));
	fs.block_size = 512;

	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFREG | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 4096;

	node = fstree_mknode(NULL, "filename", 8, "input", &sb);
	assert(node->uid == sb.st_uid);
	assert(node->gid == sb.st_gid);
	assert(node->mode == sb.st_mode);
	assert(node->parent == NULL);
	assert((char *)node->name >= (char *)node->payload);
	assert(node->data.file.input_file >= (char *)node->payload);
	assert(node->data.file.input_file >= node->name + 8);
	assert(strcmp(node->name, "filename") == 0);
	assert(strcmp(node->data.file.input_file, "input") == 0);
	free(node);

	return EXIT_SUCCESS;
}
