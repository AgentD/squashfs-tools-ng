/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * serialize_fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static sqfs_inode_generic_t *tree_node_to_inode(tree_node_t *node)
{
	sqfs_inode_generic_t *inode;
	size_t extra = 0;

	if (S_ISLNK(node->mode))
		extra = strlen(node->data.slink_target);

	inode = alloc_flex(sizeof(*inode), 1, extra);
	if (inode == NULL) {
		perror("creating inode");
		return NULL;
	}

	switch (node->mode & S_IFMT) {
	case S_IFSOCK:
		inode->base.type = SQFS_INODE_SOCKET;
		inode->data.ipc.nlink = 1;
		break;
	case S_IFIFO:
		inode->base.type = SQFS_INODE_FIFO;
		inode->data.ipc.nlink = 1;
		break;
	case S_IFLNK:
		inode->base.type = SQFS_INODE_SLINK;
		inode->data.slink.nlink = 1;
		inode->data.slink.target_size = extra;
		inode->slink_target = (char *)inode->extra;
		memcpy(inode->extra, node->data.slink_target, extra);
		break;
	case S_IFBLK:
		inode->base.type = SQFS_INODE_BDEV;
		inode->data.dev.nlink = 1;
		inode->data.dev.devno = node->data.devno;
		break;
	case S_IFCHR:
		inode->base.type = SQFS_INODE_CDEV;
		inode->data.dev.nlink = 1;
		inode->data.dev.devno = node->data.devno;
		break;
	default:
		assert(0);
	}

	return inode;
}

static sqfs_inode_generic_t *write_dir_entries(const char *filename,
					       sqfs_dir_writer_t *dirw,
					       tree_node_t *node)
{
	sqfs_u32 xattr, parent_inode;
	sqfs_inode_generic_t *inode;
	tree_node_t *it;
	int ret;

	ret = sqfs_dir_writer_begin(dirw, 0);
	if (ret)
		goto fail;

	for (it = node->data.dir.children; it != NULL; it = it->next) {
		ret = sqfs_dir_writer_add_entry(dirw, it->name, it->inode_num,
						it->inode_ref, it->mode);
		if (ret)
			goto fail;
	}

	ret = sqfs_dir_writer_end(dirw);
	if (ret)
		goto fail;

	xattr = node->xattr_idx;
	parent_inode = (node->parent == NULL) ? 0 : node->parent->inode_num;

	inode = sqfs_dir_writer_create_inode(dirw, 0, xattr, parent_inode);
	if (inode == NULL) {
		ret = SQFS_ERROR_ALLOC;
		goto fail;
	}

	return inode;
fail:
	sqfs_perror(filename, "recoding directory entries", ret);
	return NULL;
}

int sqfs_serialize_fstree(const char *filename, sqfs_file_t *file,
			  sqfs_super_t *super, fstree_t *fs,
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
	if (im == NULL) {
		ret = SQFS_ERROR_ALLOC;
		goto out_err;
	}

	dm = sqfs_meta_writer_create(file, cmp,
				     SQFS_META_WRITER_KEEP_IN_MEMORY);
	if (dm == NULL) {
		ret = SQFS_ERROR_ALLOC;
		goto out_im;
	}

	dirwr = sqfs_dir_writer_create(dm);
	if (dirwr == NULL) {
		ret = SQFS_ERROR_ALLOC;
		goto out_dm;
	}

	super->inode_table_start = file->get_size(file);

	for (i = 0; i < fs->inode_tbl_size; ++i) {
		n = fs->inode_table[i];

		if (S_ISDIR(n->mode)) {
			inode = write_dir_entries(filename, dirwr, n);

			if (inode == NULL) {
				ret = 1;
				goto out;
			}
		} else if (S_ISREG(n->mode)) {
			inode = n->data.file.user_ptr;
			n->data.file.user_ptr = NULL;

			if (inode == NULL) {
				ret = SQFS_ERROR_INTERNAL;
				goto out;
			}
		} else {
			inode = tree_node_to_inode(n);

			if (inode == NULL) {
				ret = SQFS_ERROR_ALLOC;
				goto out;
			}
		}

		inode->base.mode = n->mode;
		inode->base.mod_time = n->mod_time;
		inode->base.inode_number = n->inode_num;

		sqfs_inode_set_xattr_index(inode, n->xattr_idx);

		ret = sqfs_id_table_id_to_index(idtbl, n->uid,
						&inode->base.uid_idx);
		if (ret) {
			free(inode);
			goto out;
		}

		ret = sqfs_id_table_id_to_index(idtbl, n->gid,
						&inode->base.gid_idx);
		if (ret) {
			free(inode);
			goto out;
		}

		sqfs_meta_writer_get_position(im, &block, &offset);
		fs->inode_table[i]->inode_ref = (block << 16) | offset;

		ret = sqfs_meta_writer_write_inode(im, inode);
		free(inode);

		if (ret)
			goto out;
	}

	ret = sqfs_meta_writer_flush(im);
	if (ret)
		goto out;

	ret = sqfs_meta_writer_flush(dm);
	if (ret)
		goto out;

	super->root_inode_ref = fs->root->inode_ref;
	super->directory_table_start = file->get_size(file);

	ret = sqfs_meta_write_write_to_file(dm);
	if (ret)
		goto out;

	ret = 0;
out:
	sqfs_dir_writer_destroy(dirwr);
out_dm:
	sqfs_meta_writer_destroy(dm);
out_im:
	sqfs_meta_writer_destroy(im);
out_err:
	if (ret < 0) {
		sqfs_perror(filename, "storing filesystem tree",
			    ret);
	}
	return ret;
}
