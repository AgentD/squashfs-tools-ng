/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "rdsquashfs.h"

static size_t compute_size(sqfs_inode_generic_t *inode, const char *name,
			   size_t block_size)
{
	size_t size = sizeof(tree_node_t) + strlen(name) + 1;
	size_t block_count = 0;

	switch (inode->base.type) {
	case SQFS_INODE_DIR:
	case SQFS_INODE_EXT_DIR:
		size += sizeof(dir_info_t);
		break;
	case SQFS_INODE_FILE:
		size += sizeof(file_info_t);
		block_count = inode->data.file.file_size / block_size;
		break;
	case SQFS_INODE_EXT_FILE:
		size += sizeof(file_info_t);
		block_count = inode->data.file_ext.file_size / block_size;
		break;
	case SQFS_INODE_SLINK:
	case SQFS_INODE_EXT_SLINK:
		size += strlen(inode->slink_target) + 1;
		break;
	default:
		break;
	}

	return size + block_count * sizeof(uint32_t);
}

static void copy_block_sizes(sqfs_inode_generic_t *inode, tree_node_t *out,
			     size_t block_size)
{
	size_t block_count = out->data.file->size / block_size;

	out->name += block_count * sizeof(uint32_t);

	if (block_count) {
		memcpy(out->data.file->blocksizes, inode->block_sizes,
		       block_count * sizeof(uint32_t));
	}
}

tree_node_t *tree_node_from_inode(sqfs_inode_generic_t *inode,
				  const id_table_t *idtbl,
				  const char *name,
				  size_t block_size)
{
	tree_node_t *out;

	if (inode->base.uid_idx >= idtbl->num_ids) {
		fputs("converting inode to fs tree node: UID out of range\n",
		      stderr);
		return NULL;
	}

	if (inode->base.gid_idx >= idtbl->num_ids) {
		fputs("converting inode to fs tree node: GID out of range\n",
		      stderr);
		return NULL;
	}

	out = calloc(1, compute_size(inode, name, block_size));
	if (out == NULL) {
		perror("converting inode to fs tree node");
		return NULL;
	}

	out->uid = idtbl->ids[inode->base.uid_idx];
	out->gid = idtbl->ids[inode->base.gid_idx];
	out->mode = inode->base.mode;
	out->type = inode->base.type;
	out->inode_num = inode->base.inode_number;
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

		copy_block_sizes(inode, out, block_size);
		break;
	case SQFS_INODE_EXT_FILE:
		out->data.file = (file_info_t *)out->payload;
		out->name += sizeof(file_info_t);

		out->data.file->size = inode->data.file_ext.file_size;
		out->data.file->startblock = inode->data.file_ext.blocks_start;
		out->data.file->fragment = inode->data.file_ext.fragment_idx;
		out->data.file->fragment_offset =
			inode->data.file_ext.fragment_offset;

		copy_block_sizes(inode, out, block_size);
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
