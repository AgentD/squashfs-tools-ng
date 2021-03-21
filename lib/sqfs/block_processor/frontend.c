/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * frontend.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static sqfs_block_t *get_new_block(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk;

	if (proc->free_list != NULL) {
		blk = proc->free_list;
		proc->free_list = blk->next;
	} else {
		blk = malloc(sizeof(*blk) + proc->max_block_size);
	}

	if (blk != NULL)
		memset(blk, 0, sizeof(*blk));

	return blk;
}

static int dequeue_block(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk, *fragblk, *it, *prev;
	bool have_dequeued = false;
	int status;
retry:
	while (proc->io_queue != NULL) {
		if (proc->io_queue->io_seq_num != proc->io_deq_seq_num)
			break;

		blk = proc->io_queue;
		proc->io_queue = blk->next;
		proc->io_deq_seq_num += 1;
		proc->backlog -= 1;
		have_dequeued = true;

		status = process_completed_block(proc, blk);
		if (status != 0)
			return status;
	}

	if (have_dequeued)
		return 0;

	blk = proc->pool->dequeue(proc->pool);

	if (blk == NULL) {
		status = proc->pool->get_status(proc->pool);
		if (status == 0)
			status = SQFS_ERROR_INTERNAL;

		return status;
	}

	proc->backlog -= 1;
	have_dequeued = true;

	if (blk->flags & SQFS_BLK_IS_FRAGMENT) {
		fragblk = NULL;
		status = process_completed_fragment(proc, blk, &fragblk);

		if (status != 0) {
			free(fragblk);
			return status;
		}

		if (fragblk != NULL) {
			fragblk->io_seq_num = proc->io_seq_num++;

			if (proc->pool->submit(proc->pool, fragblk) != 0) {
				free(fragblk);

				if (status == 0) {
					status = proc->pool->
						get_status(proc->pool);

					if (status == 0)
						status = SQFS_ERROR_ALLOC;
				}

				return status;
			}

			proc->backlog += 1;
			have_dequeued = false;
		}
	} else {
		if (!(blk->flags & SQFS_BLK_FRAGMENT_BLOCK))
			blk->io_seq_num = proc->io_seq_num++;

		prev = NULL;
		it = proc->io_queue;

		while (it != NULL) {
			if (it->io_seq_num >= blk->io_seq_num)
				break;

			prev = it;
			it = it->next;
		}

		if (prev == NULL) {
			blk->next = proc->io_queue;
			proc->io_queue = blk;
		} else {
			blk->next = prev->next;
			prev->next = blk;
		}

		proc->backlog += 1;
		have_dequeued = false;
	}

	if (!have_dequeued)
		goto retry;

	return 0;
}

static int enqueue_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	int status;

	if (proc->pool->submit(proc->pool, blk) != 0) {
		status = proc->pool->get_status(proc->pool);

		if (status == 0)
			status = SQFS_ERROR_ALLOC;

		free(blk);
		return status;
	}

	proc->backlog += 1;
	return 0;
}

static int add_sentinel_block(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk;
	int ret;

	if (proc->backlog == proc->max_backlog) {
		ret = dequeue_block(proc);
		if (ret != 0)
			return ret;
	}

	blk = get_new_block(proc);
	if (blk == NULL)
		return SQFS_ERROR_ALLOC;

	blk->inode = proc->inode;
	blk->flags = proc->blk_flags | SQFS_BLK_LAST_BLOCK;

	return enqueue_block(proc, blk);
}

static int flush_block(sqfs_block_processor_t *proc)
{
	sqfs_block_t *block;
	int ret;

	if (proc->backlog == proc->max_backlog) {
		ret = dequeue_block(proc);
		if (ret != 0)
			return ret;
	}

	block = proc->blk_current;
	proc->blk_current = NULL;

	return enqueue_block(proc, block);
}

int sqfs_block_processor_begin_file(sqfs_block_processor_t *proc,
				    sqfs_inode_generic_t **inode,
				    void *user, sqfs_u32 flags)
{
	if (proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (flags & ~SQFS_BLK_USER_SETTABLE_FLAGS)
		return SQFS_ERROR_UNSUPPORTED;

	if (inode != NULL) {
		(*inode) = calloc(1, sizeof(sqfs_inode_generic_t));
		if ((*inode) == NULL)
			return SQFS_ERROR_ALLOC;

		(*inode)->base.type = SQFS_INODE_FILE;
		sqfs_inode_set_frag_location(*inode, 0xFFFFFFFF, 0xFFFFFFFF);
	}

	proc->begin_called = true;
	proc->inode = inode;
	proc->blk_flags = flags | SQFS_BLK_FIRST_BLOCK;
	proc->blk_index = 0;
	proc->user = user;
	return 0;
}

int sqfs_block_processor_append(sqfs_block_processor_t *proc, const void *data,
				size_t size)
{
	sqfs_block_t *new;
	sqfs_u64 filesize;
	size_t diff;
	int err;

	if (!proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (proc->inode != NULL) {
		sqfs_inode_get_file_size(*(proc->inode), &filesize);
		sqfs_inode_set_file_size(*(proc->inode), filesize + size);
	}

	while (size > 0) {
		if (proc->blk_current == NULL) {
			new = get_new_block(proc);
			if (new == NULL)
				return SQFS_ERROR_ALLOC;

			proc->blk_current = new;
			proc->blk_current->flags = proc->blk_flags;
			proc->blk_current->inode = proc->inode;
			proc->blk_current->user = proc->user;
			proc->blk_current->index = proc->blk_index++;
			proc->blk_flags &= ~SQFS_BLK_FIRST_BLOCK;
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

	if (!proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (proc->blk_current == NULL) {
		if (!(proc->blk_flags & SQFS_BLK_FIRST_BLOCK)) {
			err = add_sentinel_block(proc);
			if (err)
				return err;
		}
	} else {
		if (proc->blk_flags & SQFS_BLK_DONT_FRAGMENT) {
			proc->blk_current->flags |= SQFS_BLK_LAST_BLOCK;
		} else {
			if (!(proc->blk_current->flags &
			      SQFS_BLK_FIRST_BLOCK)) {
				err = add_sentinel_block(proc);
				if (err)
					return err;
			}

			proc->blk_current->flags |= SQFS_BLK_IS_FRAGMENT;
		}

		err = flush_block(proc);
		if (err)
			return err;
	}

	proc->begin_called = false;
	proc->inode = NULL;
	proc->user = NULL;
	proc->blk_flags = 0;
	return 0;
}

int sqfs_block_processor_submit_block(sqfs_block_processor_t *proc, void *user,
				      sqfs_u32 flags, const void *data,
				      size_t size)
{
	sqfs_block_t *blk;
	int ret;

	if (proc->begin_called)
		return SQFS_ERROR_SEQUENCE;

	if (size > proc->max_block_size)
		return SQFS_ERROR_OVERFLOW;

	if (flags & ~SQFS_BLK_FLAGS_ALL)
		return SQFS_ERROR_UNSUPPORTED;

	if (proc->backlog == proc->max_backlog) {
		ret = dequeue_block(proc);
		if (ret != 0)
			return ret;
	}

	blk = get_new_block(proc);
	if (blk == NULL)
		return SQFS_ERROR_ALLOC;

	blk->flags = flags | BLK_FLAG_MANUAL_SUBMISSION;
	blk->user = user;
	blk->size = size;
	memcpy(blk->data, data, size);

	ret = proc->pool->submit(proc->pool, blk);
	if (ret != 0)
		free(blk);

	proc->backlog += 1;
	return ret;
}

int sqfs_block_processor_sync(sqfs_block_processor_t *proc)
{
	int ret;

	while (proc->backlog > 0) {
		ret = dequeue_block(proc);
		if (ret != 0)
			return ret;
	}

	return 0;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk;
	int status;

	status = sqfs_block_processor_sync(proc);
	if (status != 0)
		return status;

	if (proc->frag_block != NULL) {
		blk = proc->frag_block;
		blk->next = NULL;
		proc->frag_block = NULL;

		blk->io_seq_num = proc->io_seq_num++;

		status = proc->pool->submit(proc->pool, blk);
		if (status != 0) {
			status = proc->pool->get_status(proc->pool);

			if (status == 0)
				status = SQFS_ERROR_ALLOC;

			free(blk);
			return status;
		}

		proc->backlog += 1;
		status = sqfs_block_processor_sync(proc);
	}

	return status;
}

const sqfs_block_processor_stats_t
*sqfs_block_processor_get_stats(const sqfs_block_processor_t *proc)
{
	return &proc->stats;
}
