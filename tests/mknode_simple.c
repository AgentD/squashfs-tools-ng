/* SPDX-License-Identifier: GPL-3.0-or-later */
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
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFSOCK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(&fs, NULL, "sockfile", 8, (void *)0x1000, &sb);
	assert((char *)node->name >= (char *)node->payload);
	assert(strcmp(node->name, "sockfile") == 0);
	assert(node->uid == sb.st_uid);
	assert(node->gid == sb.st_gid);
	assert(node->mode == sb.st_mode);
	assert(node->parent == NULL);
	assert(node->data.dir == NULL);
	assert(node->data.file == NULL);
	assert(node->data.slink_target == NULL);
	assert(node->data.devno == 0);
	free(node);

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFIFO | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(&fs, NULL, "fifo", 4, (void *)0x1000, &sb);
	assert((char *)node->name >= (char *)node->payload);
	assert(strcmp(node->name, "fifo") == 0);
	assert(node->uid == sb.st_uid);
	assert(node->gid == sb.st_gid);
	assert(node->mode == sb.st_mode);
	assert(node->parent == NULL);
	assert(node->data.dir == NULL);
	assert(node->data.file == NULL);
	assert(node->data.slink_target == NULL);
	assert(node->data.devno == 0);
	free(node);

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFBLK | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(&fs, NULL, "blkdev", 6, (void *)0x1000, &sb);
	assert((char *)node->name >= (char *)node->payload);
	assert(strcmp(node->name, "blkdev") == 0);
	assert(node->uid == sb.st_uid);
	assert(node->gid == sb.st_gid);
	assert(node->mode == sb.st_mode);
	assert(node->data.devno == sb.st_rdev);
	assert(node->parent == NULL);
	free(node);

	memset(&fs, 0, sizeof(fs));
	memset(&sb, 0, sizeof(sb));
	sb.st_mode = S_IFCHR | 0654;
	sb.st_uid = 123;
	sb.st_gid = 456;
	sb.st_rdev = 789;
	sb.st_size = 1337;

	node = fstree_mknode(&fs, NULL, "chardev", 7, (void *)0x1000, &sb);
	assert((char *)node->name >= (char *)node->payload);
	assert(strcmp(node->name, "chardev") == 0);
	assert(node->uid == sb.st_uid);
	assert(node->gid == sb.st_gid);
	assert(node->mode == sb.st_mode);
	assert(node->data.devno == sb.st_rdev);
	assert(node->parent == NULL);
	free(node);

	return EXIT_SUCCESS;
}
