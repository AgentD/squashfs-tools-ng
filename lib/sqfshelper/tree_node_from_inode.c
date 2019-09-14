/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tree_node_from_inode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static size_t compute_size(sqfs_inode_generic_t *inode, const char *name)
{
	size_t size = sizeof(tree_node_t) + strlen(name) + 1;

	switch (inode->base.type) {
	case SQFS_INODE_DIR:
	case SQFS_INODE_EXT_DIR:
		size += sizeof(dir_info_t);
		break;
	case SQFS_INODE_FILE:
	case SQFS_INODE_EXT_FILE:
		size += sizeof(file_info_t);
		size += inode->num_file_blocks *
			sizeof(((file_info_t *)0)->block_size[0]);
		break;
	case SQFS_INODE_SLINK:
	case SQFS_INODE_EXT_SLINK:
		size += strlen(inode->slink_target) + 1;
		break;
	default:
		break;
	}

	return size;
}

static void copy_block_sizes(sqfs_inode_generic_t *inode, tree_node_t *out)
{
	size_t i;

	out->name += inode->num_file_blocks *
		sizeof(out->data.file->block_size[0]);

	for (i = 0; i < inode->num_file_blocks; ++i)
		out->data.file->block_size[i] = inode->block_sizes[i];
}

tree_node_t *tree_node_from_inode(sqfs_inode_generic_t *inode,
				  const sqfs_id_table_t *idtbl,
				  const char *name)
{
	tree_node_t *out;

	out = calloc(1, compute_size(inode, name));
	if (out == NULL) {
		perror("converting inode to fs tree node");
		return NULL;
	}

	if (sqfs_id_table_index_to_id(idtbl, inode->base.uid_idx, &out->uid)) {
		free(out);
		return NULL;
	}

	if (sqfs_id_table_index_to_id(idtbl, inode->base.gid_idx, &out->gid)) {
		free(out);
		return NULL;
	}

	out->mode = inode->base.mode;
	out->inode_num = inode->base.inode_number;
	out->mod_time = inode->base.mod_time;
	out->name = (char *)out->payload;

	switch (inode->base.type) {
	case SQFS_INODE_DIR:
		out->data.dir = (dir_info_t *)out->payload;
		out->name += sizeof(dir_info_t);

		out->data.dir->size = inode->data.dir.size;
		out->data.dir->start_block = inode->data.dir.start_block;
		out->data.dir->block_offset = inode->data.dir.offset;
		break;
	case SQFS_INODE_EXT_DIR:
		out->data.dir = (dir_info_t *)out->payload;
		out->name += sizeof(dir_info_t);

		out->data.dir->size = inode->data.dir_ext.size;
		out->data.dir->start_block = inode->data.dir_ext.start_block;
		out->data.dir->block_offset = inode->data.dir_ext.offset;
		break;
	case SQFS_INODE_FILE:
		out->data.file = (file_info_t *)out->payload;
		out->name += sizeof(file_info_t);

		out->data.file->size = inode->data.file.file_size;
		out->data.file->startblock = inode->data.file.blocks_start;
		out->data.file->fragment = inode->data.file.fragment_index;
		out->data.file->fragment_offset =
			inode->data.file.fragment_offset;

		copy_block_sizes(inode, out);
		break;
	case SQFS_INODE_EXT_FILE:
		out->data.file = (file_info_t *)out->payload;
		out->name += sizeof(file_info_t);

		out->data.file->size = inode->data.file_ext.file_size;
		out->data.file->sparse = inode->data.file_ext.sparse;
		out->data.file->startblock = inode->data.file_ext.blocks_start;
		out->data.file->fragment = inode->data.file_ext.fragment_idx;
		out->data.file->fragment_offset =
			inode->data.file_ext.fragment_offset;

		copy_block_sizes(inode, out);
		break;
	case SQFS_INODE_SLINK:
	case SQFS_INODE_EXT_SLINK:
		out->data.slink_target = (char *)out->payload;
		strcpy(out->data.slink_target, inode->slink_target);

		out->name = (char *)out->payload +
			strlen(inode->slink_target) + 1;
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		out->name = (char *)out->payload;
		out->data.devno = inode->data.dev.devno;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		out->name = (char *)out->payload;
		out->data.devno = inode->data.dev_ext.devno;
		break;
	default:
		break;
	}

	strcpy(out->name, name);
	return out;
}
