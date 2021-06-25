/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * inode_stat.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int inode_stat(const sqfs_tree_node_t *node, struct stat *sb)
{
	memset(sb, 0, sizeof(*sb));

	sb->st_mode = node->inode->base.mode;
	sb->st_uid = node->uid;
	sb->st_gid = node->gid;
	sb->st_atime = node->inode->base.mod_time;
	sb->st_mtime = node->inode->base.mod_time;
	sb->st_ctime = node->inode->base.mod_time;
	sb->st_ino = node->inode->base.inode_number;
	sb->st_nlink = 1;
	sb->st_blksize = 512;

	switch (node->inode->base.type) {
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		sb->st_rdev = node->inode->data.dev.devno;
		sb->st_nlink = node->inode->data.dev.nlink;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		sb->st_rdev = node->inode->data.dev_ext.devno;
		sb->st_nlink = node->inode->data.dev_ext.nlink;
		break;
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		sb->st_nlink = node->inode->data.ipc.nlink;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		sb->st_nlink = node->inode->data.ipc_ext.nlink;
		break;
	case SQFS_INODE_SLINK:
		sb->st_size = node->inode->data.slink.target_size;
		sb->st_nlink = node->inode->data.slink.nlink;
		break;
	case SQFS_INODE_EXT_SLINK:
		sb->st_size = node->inode->data.slink_ext.target_size;
		sb->st_nlink = node->inode->data.slink_ext.nlink;
		break;
	case SQFS_INODE_FILE:
		sb->st_size = node->inode->data.file.file_size;
		break;
	case SQFS_INODE_EXT_FILE:
		sb->st_size = node->inode->data.file_ext.file_size;
		sb->st_nlink = node->inode->data.file_ext.nlink;
		break;
	case SQFS_INODE_DIR:
		sb->st_size = node->inode->data.dir.size;
		sb->st_nlink = node->inode->data.dir.nlink;
		break;
	case SQFS_INODE_EXT_DIR:
		sb->st_size = node->inode->data.dir_ext.size;
		sb->st_nlink = node->inode->data.dir_ext.nlink;
		break;
	default:
		break;
	}

	sb->st_blocks = sb->st_size / sb->st_blksize;
	if (sb->st_size % sb->st_blksize)
		sb->st_blocks += 1;

	return 0;
}
