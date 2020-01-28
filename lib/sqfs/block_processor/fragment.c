/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fragtbl.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

int process_completed_fragment(sqfs_block_processor_t *proc, sqfs_block_t *frag,
			       sqfs_block_t **blk_out)
{
	sqfs_u32 index, offset;
	size_t size;
	int err;

	err = sqfs_frag_table_find_tail_end(proc->frag_tbl,
					    frag->checksum, frag->size,
					    &index, &offset);
	if (err == 0)
		goto out_duplicate;

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

	err = sqfs_frag_table_add_tail_end(proc->frag_tbl,
					   proc->frag_block->index,
					   proc->frag_block->size,
					   frag->size, frag->checksum);
	if (err)
		goto fail;

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
fail:
	free(*blk_out);
	*blk_out = NULL;
	return err;
out_duplicate:
	sqfs_inode_set_frag_location(frag->inode, index, offset);

	if (proc->hooks != NULL &&
	    proc->hooks->notify_fragment_discard != NULL) {
		proc->hooks->notify_fragment_discard(proc->user_ptr, frag);
	}
	return 0;
}
