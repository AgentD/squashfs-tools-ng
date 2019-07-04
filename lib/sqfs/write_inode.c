/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "meta_writer.h"
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

static size_t hard_link_count(tree_node_t *n)
{
	size_t count;

	if (S_ISDIR(n->mode)) {
		count = 2;

		for (n = n->data.dir->children; n != NULL; n = n->next)
			++count;

		return count;
	}

	return 1;
}

int meta_writer_write_inode(fstree_t *fs, id_table_t *idtbl, meta_writer_t *im,
			    meta_writer_t *dm, tree_node_t *node)
{
	dir_index_t *diridx = NULL;
	uint16_t uid_idx, gid_idx;
	file_info_t *fi = NULL;
	dir_info_t *di = NULL;
	uint32_t bs, offset;
	sqfs_inode_t base;
	uint64_t i, block;
	int type;

	if (id_table_id_to_index(idtbl, node->uid, &uid_idx))
		return -1;

	if (id_table_id_to_index(idtbl, node->gid, &gid_idx))
		return -1;

	meta_writer_get_position(im, &block, &offset);
	node->inode_ref = (block << 16) | offset;

	if (S_ISDIR(node->mode)) {
		di = node->data.dir;

		if (meta_writer_write_dir(dm, di, &diridx))
			return -1;

		if ((di->start_block) > 0xFFFFFFFFUL || di->size > 0xFFFF ||
		    node->xattr != NULL) {
			type = SQFS_INODE_EXT_DIR;
		} else {
			type = SQFS_INODE_DIR;
			free(diridx);
			diridx = NULL;
		}
	} else if (S_ISREG(node->mode)) {
		fi = node->data.file;
		type = SQFS_INODE_FILE;

		if (fi->startblock > 0xFFFFFFFFUL || fi->size > 0xFFFFFFFFUL ||
		    hard_link_count(node) > 1 || fi->sparse > 0 ||
		    node->xattr != NULL) {
			type = SQFS_INODE_EXT_FILE;
		}
	} else {
		type = get_type(node);
	}

	base.type = htole16(type);
	base.mode = htole16(node->mode);
	base.uid_idx = htole16(uid_idx);
	base.gid_idx = htole16(gid_idx);
	base.mod_time = htole32(fs->defaults.st_mtime);
	base.inode_number = htole32(node->inode_num);

	if (meta_writer_append(im, &base, sizeof(base))) {
		free(diridx);
		return -1;
	}

	switch (type) {
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET: {
		sqfs_inode_ipc_t ipc = {
			.nlink = htole32(hard_link_count(node)),
		};

		return meta_writer_append(im, &ipc, sizeof(ipc));
	}
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET: {
		sqfs_inode_ipc_ext_t ipc = {
			.nlink = htole32(hard_link_count(node)),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (node->xattr != NULL)
			ipc.xattr_idx = htole32(node->xattr->index);

		return meta_writer_append(im, &ipc, sizeof(ipc));
	}
	case SQFS_INODE_SLINK: {
		sqfs_inode_slink_t slink = {
			.nlink = htole32(hard_link_count(node)),
			.target_size = htole32(strlen(node->data.slink_target)),
		};

		if (meta_writer_append(im, &slink, sizeof(slink)))
			return -1;
		if (meta_writer_append(im, node->data.slink_target,
				       le32toh(slink.target_size))) {
			return -1;
		}
		break;
	}
	case SQFS_INODE_EXT_SLINK: {
		sqfs_inode_slink_t slink = {
			.nlink = htole32(hard_link_count(node)),
			.target_size = htole32(strlen(node->data.slink_target)),
		};
		uint32_t xattr = htole32(0xFFFFFFFF);

		if (node->xattr != NULL)
			xattr = htole32(node->xattr->index);

		if (meta_writer_append(im, &slink, sizeof(slink)))
			return -1;
		if (meta_writer_append(im, node->data.slink_target,
				       le32toh(slink.target_size))) {
			return -1;
		}
		if (meta_writer_append(im, &xattr, sizeof(xattr)))
			return -1;
		break;
	}
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV: {
		sqfs_inode_dev_t dev = {
			.nlink = htole32(hard_link_count(node)),
			.devno = htole32(node->data.devno),
		};

		return meta_writer_append(im, &dev, sizeof(dev));
	}
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV: {
		sqfs_inode_dev_ext_t dev = {
			.nlink = htole32(hard_link_count(node)),
			.devno = htole32(node->data.devno),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (node->xattr != NULL)
			dev.xattr_idx = htole32(node->xattr->index);

		return meta_writer_append(im, &dev, sizeof(dev));
	}

	case SQFS_INODE_EXT_FILE: {
		sqfs_inode_file_ext_t ext = {
			.blocks_start = htole64(fi->startblock),
			.file_size = htole64(fi->size),
			.sparse = htole64(fi->sparse),
			.nlink = htole32(hard_link_count(node)),
			.fragment_idx = htole32(0xFFFFFFFF),
			.fragment_offset = htole32(0xFFFFFFFF),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if ((fi->size % fs->block_size) != 0) {
			ext.fragment_idx = htole32(fi->fragment);
			ext.fragment_offset = htole32(fi->fragment_offset);
		}

		if (node->xattr != NULL)
			ext.xattr_idx = htole32(node->xattr->index);

		if (meta_writer_append(im, &ext, sizeof(ext)))
			return -1;
		goto out_file_blocks;
	}
	case SQFS_INODE_FILE: {
		sqfs_inode_file_t reg = {
			.blocks_start = htole32(fi->startblock),
			.fragment_index = htole32(0xFFFFFFFF),
			.fragment_offset = htole32(0xFFFFFFFF),
			.file_size = htole32(fi->size),
		};

		if ((fi->size % fs->block_size) != 0) {
			reg.fragment_index = htole32(fi->fragment);
			reg.fragment_offset = htole32(fi->fragment_offset);
		}

		if (meta_writer_append(im, &reg, sizeof(reg)))
			return -1;
		goto out_file_blocks;
	}
	case SQFS_INODE_DIR: {
		sqfs_inode_dir_t dir = {
			.start_block = htole32(node->data.dir->start_block),
			.nlink = htole32(hard_link_count(node)),
			.size = htole16(node->data.dir->size),
			.offset = htole16(node->data.dir->block_offset),
			.parent_inode = node->parent ?
				htole32(node->parent->inode_num) : htole32(1),
		};

		return meta_writer_append(im, &dir, sizeof(dir));
	}
	case SQFS_INODE_EXT_DIR: {
		sqfs_dir_index_t idx;
		size_t i;
		sqfs_inode_dir_ext_t ext = {
			.nlink = htole32(hard_link_count(node)),
			.size = htole32(node->data.dir->size),
			.start_block = htole32(node->data.dir->start_block),
			.parent_inode = node->parent ?
				htole32(node->parent->inode_num) : htole32(1),
			.inodex_count = htole32(0),
			.offset = htole16(node->data.dir->block_offset),
			.xattr_idx = htole32(0xFFFFFFFF),
		};

		if (node->xattr != NULL)
			ext.xattr_idx = htole32(node->xattr->index);

		if (meta_writer_append(im, &ext, sizeof(ext))) {
			free(diridx);
			return -1;
		}

		/* HACK: truncated index for empty directories */
		if (node->data.dir->size == 0)
			break;

		ext.inodex_count = htole32(diridx->num_nodes - 1);

		for (i = 0; i < diridx->num_nodes; ++i) {
			idx.start_block = htole32(diridx->idx_nodes[i].block);
			idx.index = htole32(diridx->idx_nodes[i].offset);
			idx.size = strlen(diridx->idx_nodes[i].node->name) - 1;
			idx.size = htole32(idx.size);

			if (meta_writer_append(im, &idx, sizeof(idx))) {
				free(diridx);
				return -1;
			}

			if (meta_writer_append(im,
					       diridx->idx_nodes[i].node->name,
					       le32toh(idx.size) + 1)) {
				free(diridx);
				return -1;
			}
		}

		free(diridx);
		break;
	}
	default:
		assert(0);
	}
	return 0;
out_file_blocks:
	for (i = 0; i < fi->size / fs->block_size; ++i) {
		bs = htole32(fi->blocksizes[i]);

		if (meta_writer_append(im, &bs, sizeof(bs)))
			return -1;
	}

	if ((fi->size % fs->block_size) != 0 &&
	    (fi->fragment == 0xFFFFFFFF || fi->fragment_offset == 0xFFFFFFFF)) {
		bs = htole32(0);

		if (meta_writer_append(im, &bs, sizeof(bs)))
			return -1;
	}
	return 0;
}
