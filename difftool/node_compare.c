/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * node_compare.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

int node_compare(sqfsdiff_t *sd, sqfs_tree_node_t *a, sqfs_tree_node_t *b)
{
	char *path = sqfs_tree_node_get_path(a);
	sqfs_tree_node_t *ait, *bit;
	int ret, status = 0;

	if (path == NULL)
		return -1;

	if (a->inode->base.type != b->inode->base.type) {
		fprintf(stdout, "%s has a different type\n", path);
		free(path);
		return 1;
	}

	if (!(sd->compare_flags & COMPARE_NO_PERM)) {
		if ((a->inode->base.mode & ~S_IFMT) !=
		    (b->inode->base.mode & ~S_IFMT)) {
			fprintf(stdout, "%s has different permissions\n",
				path);
			status = 1;
		}
	}

	if (!(sd->compare_flags & COMPARE_NO_OWNER)) {
		if (a->uid != b->uid || a->gid != b->gid) {
			fprintf(stdout, "%s has different ownership\n", path);
			status = 1;
		}
	}

	if (sd->compare_flags & COMPARE_TIMESTAMP) {
		if (a->inode->base.mod_time != b->inode->base.mod_time) {
			fprintf(stdout, "%s has a different timestamp\n", path);
			status = 1;
		}
	}

	if (sd->compare_flags & COMPARE_INODE_NUM) {
		if (a->inode->base.inode_number !=
		    b->inode->base.inode_number) {
			fprintf(stdout, "%s has a different inode number\n",
				path);
			status = 1;
		}
	}

	switch (a->inode->base.type) {
	case SQFS_INODE_SOCKET:
	case SQFS_INODE_EXT_SOCKET:
	case SQFS_INODE_FIFO:
	case SQFS_INODE_EXT_FIFO:
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		if (a->inode->data.dev.devno != b->inode->data.dev.devno) {
			fprintf(stdout, "%s has different device number\n",
				path);
			status = 1;
		}
		break;
	case SQFS_INODE_EXT_BDEV:
	case SQFS_INODE_EXT_CDEV:
		if (a->inode->data.dev_ext.devno !=
		    b->inode->data.dev_ext.devno) {
			fprintf(stdout, "%s has different device number\n",
				path);
			status = 1;
		}
		break;
	case SQFS_INODE_SLINK:
	case SQFS_INODE_EXT_SLINK:
		if (strcmp(a->inode->slink_target, b->inode->slink_target)) {
			fprintf(stdout, "%s has a different link target\n",
				path);
		}
		break;
	case SQFS_INODE_DIR:
	case SQFS_INODE_EXT_DIR:
		ret = compare_dir_entries(sd, a, b);
		if (ret < 0) {
			status = -1;
			break;
		}
		if (ret > 0)
			status = 1;

		free(path);
		path = NULL;

		ait = a->children;
		bit = b->children;

		while (ait != NULL && bit != NULL) {
			ret = node_compare(sd, ait, bit);
			if (ret < 0)
				return -1;
			if (ret > 0)
				status = 1;

			ait = ait->next;
			bit = bit->next;
		}
		break;
	case SQFS_INODE_FILE:
	case SQFS_INODE_EXT_FILE:
		ret = compare_files(sd, a->inode, b->inode, path);
		if (ret < 0) {
			status = -1;
		} else if (ret > 0) {
			fprintf(stdout, "regular file %s differs\n", path);
			status = 1;
		}
		break;
	default:
		fprintf(stdout, "%s has unknown type, ignoring\n", path);
		break;
	}

	free(path);
	return status;
}
