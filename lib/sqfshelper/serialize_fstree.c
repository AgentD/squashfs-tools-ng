/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * serialize_fstree.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/meta_writer.h"
#include "highlevel.h"
#include "util.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

static sqfs_inode_generic_t *write_dir_entries(sqfs_dir_writer_t *dirw,
					       tree_node_t *node,
					       sqfs_id_table_t *idtbl)
{
	uint32_t xattr, parent_inode;
	sqfs_inode_generic_t *inode;
	tree_node_t *it;
	int ret;

	if (sqfs_dir_writer_begin(dirw))
		return NULL;

	for (it = node->data.dir.children; it != NULL; it = it->next) {
		ret = sqfs_dir_writer_add_entry(dirw, it->name, it->inode_num,
						it->inode_ref, it->mode);
		if (ret)
			return NULL;
	}

	if (sqfs_dir_writer_end(dirw))
		return NULL;

	xattr = (node->xattr == NULL) ? 0xFFFFFFFF : node->xattr->index;
	parent_inode = (node->parent == NULL) ? 1 : node->parent->inode_num;

	inode = sqfs_dir_writer_create_inode(dirw, 0, xattr, parent_inode);
	if (inode == NULL) {
		perror("creating inode");
		return NULL;
	}

	if (sqfs_id_table_id_to_index(idtbl, node->uid, &inode->base.uid_idx))
		goto fail_id;

	if (sqfs_id_table_id_to_index(idtbl, node->gid, &inode->base.gid_idx))
		goto fail_id;

	inode->base.mode = node->mode;
	inode->base.mod_time = node->mod_time;
	inode->base.inode_number = node->inode_num;
	return inode;
fail_id:
	fputs("failed to allocate IDs\n", stderr);
	free(inode);
	return NULL;
}

int sqfs_serialize_fstree(sqfs_file_t *file, sqfs_super_t *super, fstree_t *fs,
			  sqfs_compressor_t *cmp, sqfs_id_table_t *idtbl)
{
	sqfs_inode_generic_t *inode;
	sqfs_meta_writer_t *im, *dm;
	sqfs_dir_writer_t *dirwr;
	uint32_t offset;
	uint64_t block;
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

	for (i = 2; i < fs->inode_tbl_size; ++i) {
		if (S_ISDIR(fs->inode_table[i]->mode)) {
			inode = write_dir_entries(dirwr, fs->inode_table[i],
						  idtbl);
		} else {
			inode = tree_node_to_inode(idtbl, fs->inode_table[i]);
		}

		if (inode == NULL)
			goto out;

		sqfs_meta_writer_get_position(im, &block, &offset);
		fs->inode_table[i]->inode_ref = (block << 16) | offset;

		if (sqfs_meta_writer_write_inode(im, inode)) {
			free(inode);
			goto out;
		}

		if (inode->base.type == SQFS_INODE_EXT_DIR) {
			if (sqfs_dir_writer_write_index(dirwr, im)) {
				free(inode);
				goto out;
			}
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
}
