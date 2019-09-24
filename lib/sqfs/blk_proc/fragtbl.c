/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fragtbl.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

int sqfs_block_processor_write_fragment_table(sqfs_block_processor_t *proc,
					      sqfs_super_t *super)
{
	uint64_t start;
	size_t size;
	int ret;

	if (proc->num_fragments == 0) {
		super->fragment_entry_count = 0;
		super->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
		super->flags &= ~SQFS_FLAG_ALWAYS_FRAGMENTS;
		super->flags |= SQFS_FLAG_NO_FRAGMENTS;
		return 0;
	}

	size = sizeof(proc->fragments[0]) * proc->num_fragments;
	ret = sqfs_write_table(proc->file, proc->cmp,
			       proc->fragments, size, &start);
	if (ret)
		return ret;

	super->flags &= ~SQFS_FLAG_NO_FRAGMENTS;
	super->flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	super->fragment_entry_count = proc->num_fragments;
	super->fragment_table_start = start;
	return 0;
}

static int grow_fragment_table(sqfs_block_processor_t *proc)
{
	size_t newsz;
	void *new;

	if (proc->num_fragments >= proc->max_fragments) {
		newsz = proc->max_fragments ? proc->max_fragments * 2 : 16;

		new = realloc(proc->fragments,
			      sizeof(proc->fragments[0]) * newsz);

		if (new == NULL)
			return SQFS_ERROR_ALLOC;

		proc->max_fragments = newsz;
		proc->fragments = new;
	}

	return 0;
}

static int store_fragment(sqfs_block_processor_t *proc, sqfs_block_t *frag,
			  uint64_t signature)
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

	proc->frag_list[proc->frag_list_num].index = proc->frag_block->index;
	proc->frag_list[proc->frag_list_num].offset = proc->frag_block->size;
	proc->frag_list[proc->frag_list_num].signature = signature;
	proc->frag_list_num += 1;

	sqfs_inode_set_frag_location(frag->inode, proc->frag_block->index,
				     proc->frag_block->size);

	memcpy(proc->frag_block->data + proc->frag_block->size,
	       frag->data, frag->size);

	proc->frag_block->flags |= (frag->flags & SQFS_BLK_DONT_COMPRESS);
	proc->frag_block->size += frag->size;
	return 0;
}

int handle_fragment(sqfs_block_processor_t *proc, sqfs_block_t *frag,
		    sqfs_block_t **blk_out)
{
	uint64_t signature;
	size_t i, size;
	int err;

	signature = MK_BLK_SIG(frag->checksum, frag->size);

	for (i = 0; i < proc->frag_list_num; ++i) {
		if (proc->frag_list[i].signature == signature) {
			sqfs_inode_set_frag_location(frag->inode,
						     proc->frag_list[i].index,
						     proc->frag_list[i].offset);
			return 0;
		}
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

		err = grow_fragment_table(proc);
		if (err)
			goto fail;

		proc->frag_block = calloc(1, size);
		if (proc->frag_block == NULL) {
			err = SQFS_ERROR_ALLOC;
			goto fail;
		}

		proc->frag_block->index = proc->num_fragments++;
		proc->frag_block->flags = SQFS_BLK_FRAGMENT_BLOCK;
	}

	err = store_fragment(proc, frag, signature);
	if (err)
		goto fail;

	return 0;
fail:
	free(*blk_out);
	*blk_out = NULL;
	return err;
}
