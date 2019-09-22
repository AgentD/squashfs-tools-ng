/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_inode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "sqfs/inode.h"
#include "sqfs/dir.h"
#include "util.h"

#include <assert.h>
#include <endian.h>
#include <stdlib.h>
#include <string.h>

static int get_type(tree_node_t *node)
{
	switch (node->mode & S_IFMT) {
	case S_IFSOCK:
		if (node->xattr != NULL)
			return SQFS_INODE_EXT_SOCKET;
		return SQFS_INODE_SOCKET;
	case S_IFIFO:
		if (node->xattr != NULL)
			return SQFS_INODE_EXT_FIFO;
		return SQFS_INODE_FIFO;
	case S_IFLNK:
		if (node->xattr != NULL)
			return SQFS_INODE_EXT_SLINK;
		return SQFS_INODE_SLINK;
	case S_IFBLK:
		if (node->xattr != NULL)
			return SQFS_INODE_EXT_BDEV;
		return SQFS_INODE_BDEV;
	case S_IFCHR:
		if (node->xattr != NULL)
			return SQFS_INODE_EXT_CDEV;
		return SQFS_INODE_CDEV;
	}
	assert(0);
}

sqfs_inode_generic_t *tree_node_to_inode(sqfs_id_table_t *idtbl,
					 tree_node_t *node)
{
	sqfs_inode_generic_t *inode;
	uint16_t uid_idx, gid_idx;
	uint32_t xattr = 0xFFFFFFFF;
	size_t extra = 0;

	if (S_ISREG(node->mode)) {
		inode = node->data.file->user_ptr;
		node->data.file->user_ptr = NULL;
	} else {
		if (S_ISLNK(node->mode))
			extra = strlen(node->data.slink_target);

		inode = alloc_flex(sizeof(*inode), 1, extra);
		if (inode == NULL) {
			perror("creating inode from file system tree node");
			return NULL;
		}

		if (S_ISLNK(node->mode)) {
			inode->slink_target = (char *)inode->extra;

			memcpy(inode->extra, node->data.slink_target, extra);
		}

		inode->base.type = get_type(node);
	}

	if (sqfs_id_table_id_to_index(idtbl, node->uid, &uid_idx))
		goto fail;

	if (sqfs_id_table_id_to_index(idtbl, node->gid, &gid_idx))
		goto fail;

	if (node->xattr != NULL)
		xattr = node->xattr->index;

	inode->base.mode = node->mode;
	inode->base.uid_idx = uid_idx;
	inode->base.gid_idx = gid_idx;
	inode->base.mod_time = node->mod_time;
	inode->base.inode_number = node->inode_num;

	switch (inode->base.type) {
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		inode->data.ipc.nlink = 1;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		inode->data.ipc_ext.nlink = 1;
		inode->data.ipc_ext.xattr_idx = xattr;
		break;
	case SQFS_INODE_SLINK:
		inode->data.slink.nlink = 1;
		inode->data.slink.target_size = extra;
		break;
	case SQFS_INODE_EXT_SLINK:
		inode->data.slink_ext.nlink = 1;
		inode->data.slink_ext.target_size = extra;
		inode->data.slink_ext.xattr_idx = xattr;
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		inode->data.dev.nlink = 1;
		inode->data.dev.devno = node->data.devno;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		inode->data.dev_ext.nlink = 1;
		inode->data.dev_ext.devno = node->data.devno;
		inode->data.dev_ext.xattr_idx = xattr;
		break;
	case SQFS_INODE_FILE:
		if (xattr != 0xFFFFFFFF) {
			sqfs_inode_make_extended(inode);
			inode->data.file_ext.xattr_idx = xattr;
		}
		break;
	case SQFS_INODE_EXT_FILE:
		break;
	default:
		goto fail;
	}

	return inode;
fail:
	free(inode);
	return NULL;
}
