/* SPDX-License-Identifier: GPL-3.0-or-later */
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

	node = fstree_mknode(&fs, NULL, "filename", 8, "input", &sb);
	assert(node->uid == sb.st_uid);
	assert(node->gid == sb.st_gid);
	assert(node->mode == sb.st_mode);
	assert(node->parent == NULL);
	assert((char *)node->name >= (char *)node->payload);
	assert((char *)node->data.file >= (char *)node->payload);
	assert(node->data.file->input_file > (char *)(node->data.file + 1) +
	       sizeof(uint32_t) * 4);
	assert(node->name >= node->data.file->input_file + 6);
	assert(strcmp(node->name, "filename") == 0);
	assert(strcmp(node->data.file->input_file, "input") == 0);
	free(node);

	return EXIT_SUCCESS;
}
