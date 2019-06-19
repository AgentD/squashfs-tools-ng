/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"

#include <string.h>

void fstree_node_stat(fstree_t *fs, tree_node_t *node, struct stat *sb)
{
	tree_node_t *n;

	*sb = fs->defaults;
	sb->st_ino = node->inode_num;
	sb->st_mode = node->mode;
	sb->st_nlink = 1;
	sb->st_uid = node->uid;
	sb->st_gid = node->gid;

	switch (node->mode & S_IFMT) {
	case S_IFDIR:
		sb->st_nlink = 2;

		for (n = node->data.dir->children; n != NULL; n = n->next)
			sb->st_nlink += 1;

		sb->st_size = node->data.dir->size;
		break;
	case S_IFREG:
		sb->st_size = node->data.file->size;
		break;
	case S_IFLNK:
		sb->st_size = strlen(node->data.slink_target);
		break;
	case S_IFBLK:
	case S_IFCHR:
		sb->st_rdev = node->data.devno;
		break;
	}

	sb->st_blocks = sb->st_size / fs->block_size;
}
