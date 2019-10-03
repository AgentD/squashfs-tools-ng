/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * serialize_fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_writer.h"
#include "sqfs/inode.h"
#include "sqfs/dir.h"

#include "highlevel.h"
#include "util.h"

#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int get_type(tree_node_t *node)
{
	switch (node->mode & S_IFMT) {
	case S_IFSOCK: return SQFS_INODE_SOCKET;
	case S_IFIFO: return SQFS_INODE_FIFO;
	case S_IFLNK: return SQFS_INODE_SLINK;
	case S_IFBLK: return SQFS_INODE_BDEV;
	case S_IFCHR: return SQFS_INODE_CDEV;
	}
	assert(0);
}

static sqfs_inode_generic_t *tree_node_to_inode(tree_node_t *node)
{
	sqfs_inode_generic_t *inode;
	size_t extra = 0;

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

	switch (inode->base.type) {
	case SQFS_INODE_FIFO:
	case SQFS_INODE_SOCKET:
		inode->data.ipc.nlink = 1;
		break;
	case SQFS_INODE_SLINK:
		inode->data.slink.nlink = 1;
		inode->data.slink.target_size = extra;
		break;
	case SQFS_INODE_BDEV:
	case SQFS_INODE_CDEV:
		inode->data.dev.nlink = 1;
		inode->data.dev.devno = node->data.devno;
		break;
	}

	return inode;
}

static sqfs_inode_generic_t *write_dir_entries(sqfs_dir_writer_t *dirw,
					       tree_node_t *node)
{
	sqfs_u32 xattr, parent_inode;
	sqfs_inode_generic_t *inode;
	tree_node_t *it;
	int ret;

	if (sqfs_dir_writer_begin(dirw, 0))
		return NULL;

	for (it = node->data.dir.children; it != NULL; it = it->next) {
		ret = sqfs_dir_writer_add_entry(dirw, it->name, it->inode_num,
						it->inode_ref, it->mode);
		if (ret)
			return NULL;
	}

	if (sqfs_dir_writer_end(dirw))
		return NULL;

	xattr = node->xattr_idx;
	parent_inode = (node->parent == NULL) ? 0 : node->parent->inode_num;

	inode = sqfs_dir_writer_create_inode(dirw, 0, xattr, parent_inode);
	if (inode == NULL) {
		perror("creating inode");
		return NULL;
	}

	return inode;
}

int sqfs_serialize_fstree(sqfs_file_t *file, sqfs_super_t *super, fstree_t *fs,
			  sqfs_compressor_t *cmp, sqfs_id_table_t *idtbl)
{
	sqfs_inode_generic_t *inode;
	sqfs_meta_writer_t *im, *dm;
	sqfs_dir_writer_t *dirwr;
	sqfs_u32 offset;
	sqfs_u64 block;
	tree_node_t *n;
	int ret = -1;
	size_t i;

	im = sqfs_meta_writer_create(file, cmp, 0);
	if (im == NULL)
		return -1;

	dm = sqfs_meta_writer_create(file, cmp,
				     SQFS_META_WRITER_KEEP_IN_MEMORY);
	if (dm == NULL)
		goto out_im;

	dirwr = sqfs_dir_writer_create(dm);
	if (dirwr == NULL)
		goto out_dm;

	super->inode_table_start = file->get_size(file);

	for (i = 0; i < fs->inode_tbl_size; ++i) {
		n = fs->inode_table[i];

		if (S_ISDIR(n->mode)) {
			inode = write_dir_entries(dirwr, n);
		} else if (S_ISREG(n->mode)) {
			inode = n->data.file.user_ptr;
			n->data.file.user_ptr = NULL;
		} else {
			inode = tree_node_to_inode(n);
		}

		if (inode == NULL)
			goto out;

		inode->base.mode = n->mode;
		inode->base.mod_time = n->mod_time;
		inode->base.inode_number = n->inode_num;

		sqfs_inode_set_xattr_index(inode, n->xattr_idx);

		if (sqfs_id_table_id_to_index(idtbl, n->uid,
					      &inode->base.uid_idx)) {
			goto fail_id;
		}

		if (sqfs_id_table_id_to_index(idtbl, n->gid,
					      &inode->base.gid_idx)) {
			goto fail_id;
		}

		sqfs_meta_writer_get_position(im, &block, &offset);
		fs->inode_table[i]->inode_ref = (block << 16) | offset;

		if (sqfs_meta_writer_write_inode(im, inode)) {
			free(inode);
			goto out;
		}

		free(inode);
	}

	if (sqfs_meta_writer_flush(im))
		goto out;

	if (sqfs_meta_writer_flush(dm))
		goto out;

	super->root_inode_ref = fs->root->inode_ref;
	super->directory_table_start = file->get_size(file);

	if (sqfs_meta_write_write_to_file(dm))
		goto out;

	ret = 0;
out:
	sqfs_dir_writer_destroy(dirwr);
out_dm:
	sqfs_meta_writer_destroy(dm);
out_im:
	sqfs_meta_writer_destroy(im);
	return ret;
fail_id:
	fputs("failed to allocate IDs\n", stderr);
	free(inode);
	goto out;
}
