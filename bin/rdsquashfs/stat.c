/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * stat.c
 *
 * Copyright (C) 2020 David Oberhollenzer <goliath@infraroot.at>
 */
#include "rdsquashfs.h"

static const char *inode_types[] = {
	[SQFS_INODE_DIR] = "directory",
	[SQFS_INODE_FILE] = "file",
	[SQFS_INODE_SLINK] = "symbolic link",
	[SQFS_INODE_BDEV] = "block device",
	[SQFS_INODE_CDEV] = "character device",
	[SQFS_INODE_FIFO] = "named pipe",
	[SQFS_INODE_SOCKET] = "socket",
	[SQFS_INODE_EXT_DIR] = "extended directory",
	[SQFS_INODE_EXT_FILE] = "extended file",
	[SQFS_INODE_EXT_SLINK] = "extended symbolic link",
	[SQFS_INODE_EXT_BDEV] = "extended block device",
	[SQFS_INODE_EXT_CDEV] = "extended character device",
	[SQFS_INODE_EXT_FIFO] = "extended named pipe",
	[SQFS_INODE_EXT_SOCKET] = "extended socket",
};

int stat_file(const sqfs_tree_node_t *node)
{
	sqfs_u32 xattr_idx = 0xFFFFFFFF, devno = 0, link_size = 0;
	const sqfs_inode_generic_t *inode = node->inode;
	const char *type = NULL, *link_target = NULL;
	sqfs_u32 frag_idx, frag_offset;
	bool have_devno = false;
	sqfs_u64 location, size;
	unsigned int nlinks = 0;
	sqfs_dir_index_t *idx;
	char buffer[64];
	time_t timeval;
	struct tm *tm;
	size_t i;
	int ret;

	/* decode */
	if ((size_t)inode->base.type <
	    sizeof(inode_types) / sizeof(inode_types[0])) {
		type = inode_types[inode->base.type];
	}

	sqfs_inode_get_xattr_index(inode, &xattr_idx);

	switch (inode->base.type) {
	case SQFS_INODE_DIR:
		nlinks = inode->data.dir.nlink;
		break;
	case SQFS_INODE_SLINK:
		nlinks = inode->data.slink.nlink;
		link_target = (const char *)inode->extra;
		link_size = inode->data.slink.target_size;
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		nlinks = inode->data.dev.nlink;
		devno = inode->data.dev.devno;
		have_devno = true;
		break;
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		nlinks = inode->data.ipc.nlink;
		break;
	case SQFS_INODE_EXT_DIR:
		nlinks = inode->data.dir_ext.nlink;
		break;
	case SQFS_INODE_EXT_FILE:
		nlinks = inode->data.file_ext.nlink;
		break;
	case SQFS_INODE_EXT_SLINK:
		nlinks = inode->data.slink_ext.nlink;
		link_target = (const char *)inode->extra;
		link_size = inode->data.slink_ext.target_size;
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		nlinks = inode->data.dev_ext.nlink;
		devno = inode->data.dev_ext.devno;
		have_devno = true;
		break;
	case SQFS_INODE_EXT_FIFO:
	case SQFS_INODE_EXT_SOCKET:
		nlinks = inode->data.ipc_ext.nlink;
		break;
	default:
		break;
	}

	timeval = inode->base.mod_time;
	tm = gmtime(&timeval);
	strftime(buffer, sizeof(buffer), "%a, %d %b %Y %T %z", tm);

	/* info dump */
	printf("Name: %s\n", (const char *)node->name);
	printf("Inode type: %s\n", type == NULL ? "UNKNOWN" : type);
	printf("Inode number: %u\n", inode->base.inode_number);
	printf("Access: 0%o\n",
	       (unsigned int)inode->base.mode & ~SQFS_INODE_MODE_MASK);
	printf("UID: %u (index = %u)\n", node->uid, inode->base.uid_idx);
	printf("GID: %u (index = %u)\n", node->gid, inode->base.gid_idx);
	printf("Last modified: %s (%u)\n", buffer, inode->base.mod_time);

	if (type != NULL && inode->base.type != SQFS_INODE_FILE)
		printf("Hard link count: %u\n", nlinks);

	if (type != NULL && inode->base.type >= SQFS_INODE_EXT_DIR)
		printf("Xattr index: 0x%X\n", xattr_idx);

	if (link_target != NULL)
		printf("Link target: %.*s\n", (int)link_size, link_target);

	if (have_devno) {
		printf("Device number: %u:%u (%u)\n",
		       major(devno), minor(devno), devno);
	}

	switch (inode->base.type) {
	case SQFS_INODE_FILE:
	case SQFS_INODE_EXT_FILE:
		sqfs_inode_get_file_block_start(inode, &location);
		sqfs_inode_get_file_size(inode, &size);
		sqfs_inode_get_frag_location(inode, &frag_idx, &frag_offset);

		printf("Fragment index: 0x%X\n", frag_idx);
		printf("Fragment offset: %u\n", frag_offset);
		printf("File size: %lu\n", (unsigned long)size);

		if (inode->base.type == SQFS_INODE_EXT_FILE)
			printf("Sparse: %lu\n", inode->data.file_ext.sparse);

		printf("Blocks start: %lu\n", (unsigned long)location);
		printf("Block count: %lu\n",
		       (unsigned long)sqfs_inode_get_file_block_count(inode));

		for (i = 0; i < sqfs_inode_get_file_block_count(inode); ++i) {
			printf("\tBlock #%lu size: %u (%s)\n", (unsigned long)i,
			       SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]),
			       SQFS_IS_BLOCK_COMPRESSED(inode->extra[i]) ?
			       "compressed" : "uncompressed");
		}
		break;
	case SQFS_INODE_DIR:
		printf("Start block: %u\n", inode->data.dir.start_block);
		printf("Offset: %u\n", inode->data.dir.offset);
		printf("Listing size: %u\n", inode->data.dir.size);
		printf("Parent inode: %u\n", inode->data.dir.parent_inode);
		break;
	case SQFS_INODE_EXT_DIR:
		printf("Start block: %u\n", inode->data.dir_ext.start_block);
		printf("Offset: %u\n", inode->data.dir_ext.offset);
		printf("Listing size: %u\n", inode->data.dir_ext.size);
		printf("Parent inode: %u\n", inode->data.dir_ext.parent_inode);
		printf("Directory index entries: %u\n",
		       inode->data.dir_ext.inodex_count);

		if (inode->data.dir_ext.size == 0)
			break;

		for (i = 0; ; ++i) {
			ret = sqfs_inode_unpack_dir_index_entry(inode, &idx, i);
			if (ret == SQFS_ERROR_OUT_OF_BOUNDS)
				break;
			if (ret < 0) {
				sqfs_perror(NULL, "reading directory index",
					    ret);
				return -1;
			}

			printf("\t'%.*s' -> block %u, header offset %u\n",
			       (int)(idx->size + 1), idx->name,
			       idx->start_block, idx->index);

			sqfs_free(idx);
		}
		break;
	default:
		break;
	}
	return 0;
}
