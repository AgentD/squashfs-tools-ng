/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fragtbl.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static int grow_deduplication_list(sqfs_data_writer_t *proc)
{
	size_t new_sz;
	void *new;

	if (proc->frag_list_num == proc->frag_list_max) {
		new_sz = proc->frag_list_max * 2;
		new = realloc(proc->frag_list,
			      sizeof(proc->frag_list[0]) * new_sz);

		if (new == NULL)
			return SQFS_ERROR_ALLOC;

		proc->frag_list = new;
		proc->frag_list_max = new_sz;
	}

	return 0;
}

static int store_fragment(sqfs_data_writer_t *proc, sqfs_block_t *frag,
			  sqfs_u64 hash)
{
	int err = grow_deduplication_list(proc);

	if (err)
		return err;

	proc->frag_list[proc->frag_list_num].index = proc->frag_block->index;
	proc->frag_list[proc->frag_list_num].offset = proc->frag_block->size;
	proc->frag_list[proc->frag_list_num].hash = hash;
	proc->frag_list_num += 1;

	sqfs_inode_set_frag_location(frag->inode, proc->frag_block->index,
				     proc->frag_block->size);

	if (proc->hooks != NULL && proc->hooks->pre_fragment_store != NULL) {
		proc->hooks->pre_fragment_store(proc->user_ptr, frag);
	}

	memcpy(proc->frag_block->data + proc->frag_block->size,
	       frag->data, frag->size);

	proc->frag_block->flags |= (frag->flags & SQFS_BLK_DONT_COMPRESS);
	proc->frag_block->size += frag->size;
	return 0;
}

int process_completed_fragment(sqfs_data_writer_t *proc, sqfs_block_t *frag,
			       sqfs_block_t **blk_out)
{
	sqfs_u64 hash;
	sqfs_u32 index;
	size_t i, size;
	int err;

	hash = MK_BLK_HASH(frag->checksum, frag->size);

	for (i = 0; i < proc->frag_list_num; ++i) {
		if (proc->frag_list[i].hash == hash)
			goto out_duplicate;
	}

	if (proc->frag_block != NULL) {
		size = proc->frag_block->size + frag->size;

		if (size > proc->max_block_size) {
			*blk_out = proc->frag_block;
			proc->frag_block = NULL;
		}
	}

	if (proc->frag_block == NULL) {
		size = sizeof(sqfs_block_t) + proc->max_block_size;

		err= sqfs_frag_table_append(proc->frag_tbl, 0, 0, &index);
		if (err)
			goto fail;

		proc->frag_block = calloc(1, size);
		if (proc->frag_block == NULL) {
			err = SQFS_ERROR_ALLOC;
			goto fail;
		}

		proc->frag_block->index = index;
		proc->frag_block->flags = SQFS_BLK_FRAGMENT_BLOCK;
	}

	err = store_fragment(proc, frag, hash);
	if (err)
		goto fail;

	return 0;
fail:
	free(*blk_out);
	*blk_out = NULL;
	return err;
out_duplicate:
	sqfs_inode_set_frag_location(frag->inode, proc->frag_list[i].index,
				     proc->frag_list[i].offset);

	if (proc->hooks != NULL &&
	    proc->hooks->notify_fragment_discard != NULL) {
		proc->hooks->notify_fragment_discard(proc->user_ptr, frag);
	}
	return 0;
}
