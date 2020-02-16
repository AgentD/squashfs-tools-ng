/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fileapi.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static int enqueue_block(sqfs_block_processor_t *proc, sqfs_block_t *block)
{
	int status;

	while (proc->backlog > proc->max_backlog) {
		status = wait_completed(proc);
		if (status)
			return status;
	}

	return append_to_work_queue(proc, block);
}

static int add_sentinel_block(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk = calloc(1, sizeof(*blk));

	if (blk == NULL)
		return SQFS_ERROR_ALLOC;

	blk->inode = proc->inode;
	blk->flags = proc->blk_flags | SQFS_BLK_LAST_BLOCK;

	return enqueue_block(proc, blk);
}

int sqfs_block_processor_begin_file(sqfs_block_processor_t *proc,
				    sqfs_inode_generic_t *inode, sqfs_u32 flags)
{
	if (proc->inode != NULL)
		return SQFS_ERROR_SEQUENCE;

	if (flags & ~SQFS_BLK_USER_SETTABLE_FLAGS)
		return SQFS_ERROR_UNSUPPORTED;

	proc->inode = inode;
	proc->blk_flags = flags | SQFS_BLK_FIRST_BLOCK;
	return 0;
}

static int flush_block(sqfs_block_processor_t *proc)
{
	sqfs_block_t *block = proc->blk_current;

	proc->blk_current = NULL;

	if (block->size < proc->max_block_size &&
	    !(block->flags & SQFS_BLK_DONT_FRAGMENT)) {
		block->flags |= SQFS_BLK_IS_FRAGMENT;
	} else {
		proc->blk_flags &= ~SQFS_BLK_FIRST_BLOCK;
	}

	return enqueue_block(proc, block);
}

int sqfs_block_processor_append(sqfs_block_processor_t *proc, const void *data,
				size_t size)
{
	sqfs_block_t *new;
	size_t diff;
	int err;

	while (size > 0) {
		if (proc->blk_current == NULL) {
			new = alloc_flex(sizeof(*new), 1, proc->max_block_size);
			if (new == NULL)
				return SQFS_ERROR_ALLOC;

			proc->blk_current = new;
			proc->blk_current->flags = proc->blk_flags;
			proc->blk_current->inode = proc->inode;
		}

		diff = proc->max_block_size - proc->blk_current->size;

		if (diff == 0) {
			err = flush_block(proc);
			if (err)
				return err;
			continue;
		}

		if (diff > size)
			diff = size;

		memcpy(proc->blk_current->data + proc->blk_current->size,
		       data, diff);

		size -= diff;
		proc->blk_current->size += diff;
		data = (const char *)data + diff;

		proc->stats.input_bytes_read += diff;
	}

	if (proc->blk_current != NULL &&
	    proc->blk_current->size == proc->max_block_size) {
		return flush_block(proc);
	}

	return 0;
}

int sqfs_block_processor_end_file(sqfs_block_processor_t *proc)
{
	int err;

	if (proc->inode == NULL)
		return SQFS_ERROR_SEQUENCE;

	if (!(proc->blk_flags & SQFS_BLK_FIRST_BLOCK)) {
		if (proc->blk_current != NULL &&
		    (proc->blk_flags & SQFS_BLK_DONT_FRAGMENT)) {
			proc->blk_flags |= SQFS_BLK_LAST_BLOCK;
		} else {
			err = add_sentinel_block(proc);
			if (err)
				return err;
		}
	}

	if (proc->blk_current != NULL) {
		err = flush_block(proc);
		if (err)
			return err;
	}

	proc->inode = NULL;
	proc->blk_flags = 0;
	return 0;
}
