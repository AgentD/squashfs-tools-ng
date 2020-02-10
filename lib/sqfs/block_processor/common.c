/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * common.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

void free_blk_list(sqfs_block_t *list)
{
	sqfs_block_t *it;

	while (list != NULL) {
		it = list;
		list = list->next;
		free(it);
	}
}

int block_processor_init(sqfs_block_processor_t *proc, size_t max_block_size,
			 sqfs_compressor_t *cmp, unsigned int num_workers,
			 size_t max_backlog, sqfs_block_writer_t *wr,
			 sqfs_frag_table_t *tbl)
{
	proc->max_block_size = max_block_size;
	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->cmp = cmp;
	proc->frag_tbl = tbl;
	proc->wr = wr;

	memset(&proc->stats, 0, sizeof(proc->stats));
	proc->stats.size = sizeof(proc->stats);
	return 0;
}

void block_processor_cleanup(sqfs_block_processor_t *proc)
{
	free_blk_list(proc->queue);
	free_blk_list(proc->done);
	free(proc->blk_current);
	free(proc->frag_block);
	free(proc);
}

int sqfs_block_processor_set_hooks(sqfs_block_processor_t *proc, void *user_ptr,
				   const sqfs_block_hooks_t *hooks)
{
	proc->hooks = hooks;
	proc->user_ptr = user_ptr;

	return sqfs_block_writer_set_hooks(proc->wr, user_ptr, hooks);
}

int process_completed_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_u64 location;
	sqfs_u32 size;
	int err;

	size = blk->size;
	if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
		size |= 1 << 24;

	err = sqfs_block_writer_write(proc->wr, blk, &location);
	if (err)
		return err;

	if (blk->size != 0) {
		if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
			err = sqfs_frag_table_set(proc->frag_tbl, blk->index,
						  location, size);
			if (err)
				return err;
		} else {
			blk->inode->extra[blk->index] = size;
		}
	}

	if (blk->flags & SQFS_BLK_LAST_BLOCK)
		sqfs_inode_set_file_block_start(blk->inode, location);

	return 0;
}

int block_processor_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			     sqfs_u8 *scratch, size_t scratch_size)
{
	ssize_t ret;

	if (block->size == 0) {
		block->checksum = 0;
		return 0;
	}

	block->checksum = crc32(0, block->data, block->size);

	if (block->flags & SQFS_BLK_IS_FRAGMENT)
		return 0;

	if (!(block->flags & SQFS_BLK_DONT_COMPRESS)) {
		ret = cmp->do_block(cmp, block->data, block->size,
				    scratch, scratch_size);
		if (ret < 0)
			return ret;

		if (ret > 0) {
			memcpy(block->data, scratch, ret);
			block->size = ret;
			block->flags |= SQFS_BLK_IS_COMPRESSED;
		}
	}

	return 0;
}

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
		proc->stats.frag_block_count += 1;
	}

	err = sqfs_frag_table_add_tail_end(proc->frag_tbl,
					   proc->frag_block->index,
					   proc->frag_block->size,
					   frag->size, frag->checksum);
	if (err)
		goto fail;

	sqfs_inode_set_frag_location(frag->inode, proc->frag_block->index,
				     proc->frag_block->size);

	memcpy(proc->frag_block->data + proc->frag_block->size,
	       frag->data, frag->size);

	proc->frag_block->flags |= (frag->flags & SQFS_BLK_DONT_COMPRESS);
	proc->frag_block->size += frag->size;
	proc->stats.actual_frag_count += 1;
	return 0;
fail:
	free(*blk_out);
	*blk_out = NULL;
	return err;
out_duplicate:
	sqfs_inode_set_frag_location(frag->inode, index, offset);
	return 0;
}

const sqfs_block_processor_stats_t
*sqfs_block_processor_get_stats(const sqfs_block_processor_t *proc)
{
	return &proc->stats;
}
