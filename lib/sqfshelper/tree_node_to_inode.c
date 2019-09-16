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
	case S_IFREG: {
		file_info_t *fi = node->data.file;

		if (node->xattr != NULL || fi->sparse > 0)
			return SQFS_INODE_EXT_FILE;

		if (fi->startblock > 0xFFFFFFFFUL || fi->size > 0xFFFFFFFFUL)
			return SQFS_INODE_EXT_FILE;

		return SQFS_INODE_FILE;
	}
	}
	assert(0);
}

static bool has_fragment(const fstree_t *fs, const file_info_t *file)
{
	if (file->size % fs->block_size == 0)
		return false;

	return file->fragment_offset < fs->block_size &&
		(file->fragment != 0xFFFFFFFF);
}

sqfs_inode_generic_t *tree_node_to_inode(fstree_t *fs, sqfs_id_table_t *idtbl,
					 tree_node_t *node)
{
	size_t i, extra = 0, block_count = 0;
	sqfs_inode_generic_t *inode;
	uint16_t uid_idx, gid_idx;
	uint32_t xattr = 0xFFFFFFFF;
	file_info_t *fi = NULL;

	if (S_ISLNK(node->mode)) {
		extra = strlen(node->data.slink_target);
	} else if (S_ISREG(node->mode)) {
		fi = node->data.file;

		block_count = fi->size / fs->block_size;

		if ((fi->size % fs->block_size) != 0 && !has_fragment(fs, fi))
			++block_count;

		extra = block_count * sizeof(uint32_t);
	}

	inode = alloc_flex(sizeof(*inode), 1, extra);
	if (inode == NULL) {
		perror("creating inode from file system tree node");
		return NULL;
	}

	if (S_ISLNK(node->mode)) {
		inode->slink_target = (char *)inode->extra;

		memcpy(inode->extra, node->data.slink_target, extra);
	} else if (S_ISREG(node->mode)) {
		inode->block_sizes = (uint32_t *)inode->extra;

		for (i = 0; i < block_count; ++i) {
			inode->block_sizes[i] =
				node->data.file->block_size[i];
		}
	}

	if (sqfs_id_table_id_to_index(idtbl, node->uid, &uid_idx))
		goto fail;

	if (sqfs_id_table_id_to_index(idtbl, node->gid, &gid_idx))
		goto fail;

	if (node->xattr != NULL)
		xattr = node->xattr->index;

	inode->base.type = get_type(node);
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
		inode->data.file.blocks_start = fi->startblock;
		inode->data.file.fragment_index = 0xFFFFFFFF;
		inode->data.file.fragment_offset = 0xFFFFFFFF;
		inode->data.file.file_size = fi->size;

		if (has_fragment(fs, fi)) {
			inode->data.file.fragment_index = fi->fragment;
			inode->data.file.fragment_offset = fi->fragment_offset;
		}
		break;
	case SQFS_INODE_EXT_FILE:
		inode->data.file_ext.blocks_start = fi->startblock;
		inode->data.file_ext.file_size = fi->size;
		inode->data.file_ext.sparse = fi->sparse;
		inode->data.file_ext.nlink = 1;
		inode->data.file_ext.fragment_idx = 0xFFFFFFFF;
		inode->data.file_ext.fragment_offset = 0xFFFFFFFF;
		inode->data.file_ext.xattr_idx = xattr;

		if (has_fragment(fs, fi)) {
			inode->data.file_ext.fragment_idx = fi->fragment;
			inode->data.file_ext.fragment_offset =
				fi->fragment_offset;
		}
		break;
	default:
		goto fail;
	}

	inode->num_file_blocks = block_count;
	return inode;
fail:
	free(inode);
	return NULL;
}
