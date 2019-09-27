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

int data_writer_init(sqfs_data_writer_t *proc, size_t max_block_size,
		     sqfs_compressor_t *cmp, unsigned int num_workers,
		     size_t max_backlog, size_t devblksz, sqfs_file_t *file)
{
	proc->max_block_size = max_block_size;
	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->devblksz = devblksz;
	proc->cmp = cmp;
	proc->file = file;
	proc->max_blocks = INIT_BLOCK_COUNT;
	proc->frag_list_max = INIT_BLOCK_COUNT;

	proc->blocks = alloc_array(sizeof(proc->blocks[0]), proc->max_blocks);
	if (proc->blocks == NULL)
		return -1;

	proc->frag_list = alloc_array(sizeof(proc->frag_list[0]),
				      proc->frag_list_max);
	if (proc->frag_list == NULL)
		return -1;

	return 0;
}

void data_writer_cleanup(sqfs_data_writer_t *proc)
{
	free_blk_list(proc->queue);
	free_blk_list(proc->done);
	free(proc->blk_current);
	free(proc->frag_block);
	free(proc->frag_list);
	free(proc->fragments);
	free(proc->blocks);
	free(proc);
}

void data_writer_store_done(sqfs_data_writer_t *proc, sqfs_block_t *blk,
			    int status)
{
	sqfs_block_t *it = proc->done, *prev = NULL;

	while (it != NULL) {
		if (it->sequence_number >= blk->sequence_number)
			break;
		prev = it;
		it = it->next;
	}

	if (prev == NULL) {
		blk->next = proc->done;
		proc->done = blk;
	} else {
		blk->next = prev->next;
		prev->next = blk;
	}

	if (status != 0 && proc->status == 0)
		proc->status = status;

	proc->backlog -= 1;
}

sqfs_block_t *data_writer_next_work_item(sqfs_data_writer_t *proc)
{
	sqfs_block_t *blk;

	if (proc->status != 0)
		return NULL;

	blk = proc->queue;
	proc->queue = blk->next;
	blk->next = NULL;

	if (proc->queue == NULL)
		proc->queue_last = NULL;

	return blk;
}

int data_writer_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
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

int sqfs_data_writer_write_fragment_table(sqfs_data_writer_t *proc,
					  sqfs_super_t *super)
{
	sqfs_u64 start;
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

void sqfs_data_writer_set_hooks(sqfs_data_writer_t *proc, void *user_ptr,
				const sqfs_block_hooks_t *hooks)
{
	proc->hooks = hooks;
	proc->user_ptr = user_ptr;
}
