/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * write_inode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/inode.h"

static int write_block_sizes(meta_writer_t *ir, sqfs_inode_generic_t *n)
{
	uint32_t sizes[n->num_file_blocks];
	size_t i;

	for (i = 0; i < n->num_file_blocks; ++i)
		sizes[i] = htole32(n->block_sizes[i]);

	return meta_writer_append(ir, sizes,
				  sizeof(uint32_t) * n->num_file_blocks);
}

int meta_writer_write_inode(meta_writer_t *ir, sqfs_inode_generic_t *n)
{
	sqfs_inode_t base;

	base.type = htole16(n->base.type);
	base.mode = htole16(n->base.mode);
	base.uid_idx = htole16(n->base.uid_idx);
	base.gid_idx = htole16(n->base.gid_idx);
	base.mod_time = htole32(n->base.mod_time);
	base.inode_number = htole32(n->base.inode_number);

	if (meta_writer_append(ir, &base, sizeof(base)))
		return -1;

	switch (n->base.type) {
	case SQFS_INODE_DIR: {
		sqfs_inode_dir_t dir = {
			.start_block = htole32(n->data.dir.start_block),
			.nlink = htole32(n->data.dir.nlink),
			.size = htole16(n->data.dir.size),
			.offset = htole16(n->data.dir.offset),
			.parent_inode = htole32(n->data.dir.parent_inode),
		};
		return meta_writer_append(ir, &dir, sizeof(dir));
	}
	case SQFS_INODE_FILE: {
		sqfs_inode_file_t file = {
			.blocks_start = htole32(n->data.file.blocks_start),
			.fragment_index = htole32(n->data.file.fragment_index),
			.fragment_offset =
				htole32(n->data.file.fragment_offset),
			.file_size = htole32(n->data.file.file_size),
		};
		if (meta_writer_append(ir, &file, sizeof(file)))
			return -1;
		return write_block_sizes(ir, n);
	}
	case SQFS_INODE_SLINK: {
		sqfs_inode_slink_t slink = {
			.nlink = htole32(n->data.slink.nlink),
			.target_size = htole32(n->data.slink.target_size),
		};
		if (meta_writer_append(ir, &slink, sizeof(slink)))
			return -1;
		return meta_writer_append(ir, n->slink_target,
					  n->data.slink.target_size);
	}
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV: {
		sqfs_inode_dev_t dev = {
			.nlink = htole32(n->data.dev.nlink),
			.devno = htole32(n->data.dev.devno),
		};
		return meta_writer_append(ir, &dev, sizeof(dev));
	}
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET: {
		sqfs_inode_ipc_t ipc = {
			.nlink = htole32(n->data.ipc.nlink),
		};
		return meta_writer_append(ir, &ipc, sizeof(ipc));
	}
	case SQFS_INODE_EXT_DIR: {
		sqfs_inode_dir_ext_t dir = {
			.nlink = htole32(n->data.dir_ext.nlink),
			.size = htole32(n->data.dir_ext.size),
			.start_block = htole32(n->data.dir_ext.start_block),
			.parent_inode = htole32(n->data.dir_ext.parent_inode),
			.inodex_count = htole16(n->data.dir_ext.inodex_count),
			.offset = htole16(n->data.dir_ext.offset),
			.xattr_idx = htole32(n->data.dir_ext.xattr_idx),
		};
		return meta_writer_append(ir, &dir, sizeof(dir));
	}
	case SQFS_INODE_EXT_FILE: {
		sqfs_inode_file_ext_t file = {
			.blocks_start = htole64(n->data.file_ext.blocks_start),
			.file_size = htole64(n->data.file_ext.file_size),
			.sparse = htole64(n->data.file_ext.sparse),
			.nlink = htole32(n->data.file_ext.nlink),
			.fragment_idx = htole32(n->data.file_ext.fragment_idx),
			.fragment_offset =
				htole32(n->data.file_ext.fragment_offset),
			.xattr_idx = htole32(n->data.file_ext.xattr_idx),
		};
		if (meta_writer_append(ir, &file, sizeof(file)))
			return -1;
		return write_block_sizes(ir, n);
	}
	case SQFS_INODE_EXT_SLINK: {
		sqfs_inode_slink_t slink = {
			.nlink = htole32(n->data.slink_ext.nlink),
			.target_size = htole32(n->data.slink_ext.target_size),
		};
		uint32_t xattr = htole32(n->data.slink_ext.xattr_idx);

		if (meta_writer_append(ir, &slink, sizeof(slink)))
			return -1;
		if (meta_writer_append(ir, n->slink_target,
				       n->data.slink_ext.target_size)) {
			return -1;
		}
		return meta_writer_append(ir, &xattr, sizeof(xattr));
	}
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV: {
		sqfs_inode_dev_ext_t dev = {
			.nlink = htole32(n->data.dev_ext.nlink),
			.devno = htole32(n->data.dev_ext.devno),
			.xattr_idx = htole32(n->data.dev_ext.xattr_idx),
		};
		return meta_writer_append(ir, &dev, sizeof(dev));
	}
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET: {
		sqfs_inode_ipc_ext_t ipc = {
			.nlink = htole32(n->data.ipc_ext.nlink),
			.xattr_idx = htole32(n->data.ipc_ext.xattr_idx),
		};
		return meta_writer_append(ir, &ipc, sizeof(ipc));
	}
	}

	return -1;
}
