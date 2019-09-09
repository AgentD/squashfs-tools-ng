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

static int write_dir_entries(sqfs_dir_writer_t *dirw, tree_node_t *node)
{
	tree_node_t *it;
	uint64_t ref;
	int ret;

	if (sqfs_dir_writer_begin(dirw))
		return -1;

	for (it = node->data.dir->children; it != NULL; it = it->next) {
		ret = sqfs_dir_writer_add_entry(dirw, it->name, it->inode_num,
						it->inode_ref, it->mode);
		if (ret)
			return -1;
	}

	if (sqfs_dir_writer_end(dirw))
		return -1;

	ref = sqfs_dir_writer_get_dir_reference(dirw);

	node->data.dir->size = sqfs_dir_writer_get_size(dirw);
	node->data.dir->start_block = ref >> 16;
	node->data.dir->block_offset = ref & 0xFFFF;
	return 0;
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

	im = sqfs_meta_writer_create(file, cmp, false);
	if (im == NULL)
		return -1;

	dm = sqfs_meta_writer_create(file, cmp, true);
	if (dm == NULL)
		goto out_im;

	dirwr = sqfs_dir_writer_create(dm);
	if (dirwr == NULL)
		goto out_dm;

	super->inode_table_start = file->get_size(file);

	for (i = 2; i < fs->inode_tbl_size; ++i) {
		if (S_ISDIR(fs->inode_table[i]->mode)) {
			if (write_dir_entries(dirwr, fs->inode_table[i]))
				goto out;
		}

		inode = tree_node_to_inode(fs, idtbl, fs->inode_table[i]);
		if (inode == NULL)
			goto out;

		if (inode->base.type == SQFS_INODE_EXT_DIR) {
			inode->data.dir_ext.inodex_count =
				sqfs_dir_writer_get_index_size(dirwr);
		}

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
