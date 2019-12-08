/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * windows.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

struct compress_worker_t {
	sqfs_data_writer_t *shared;
	sqfs_compressor_t *cmp;
	HANDLE thread;
	sqfs_u8 scratch[];
};

static DWORD WINAPI worker_proc(LPVOID arg)
{
	compress_worker_t *worker = arg;
	sqfs_data_writer_t *shared = worker->shared;
	sqfs_block_t *blk = NULL;
	int status = 0;

	for (;;) {
		EnterCriticalSection(&shared->mtx);
		if (blk != NULL) {
			data_writer_store_done(shared, blk, status);
			WakeConditionVariable(&shared->done_cond);
		}

		while (shared->queue == NULL && shared->status == 0) {
			SleepConditionVariableCS(&shared->queue_cond,
						 &shared->mtx, INFINITE);
		}

		blk = data_writer_next_work_item(shared);
		LeaveCriticalSection(&shared->mtx);

		if (blk == NULL)
			break;

		status = data_writer_do_block(blk, worker->cmp,
					      worker->scratch,
					      shared->max_block_size);
	}

	return 0;
}

sqfs_data_writer_t *sqfs_data_writer_create(size_t max_block_size,
					    sqfs_compressor_t *cmp,
					    unsigned int num_workers,
					    size_t max_backlog,
					    size_t devblksz,
					    sqfs_file_t *file)
{
	sqfs_data_writer_t *proc;
	unsigned int i;

	if (num_workers < 1)
		num_workers = 1;

	proc = alloc_flex(sizeof(*proc),
			  sizeof(proc->workers[0]), num_workers);
	if (proc == NULL)
		return NULL;

	InitializeCriticalSection(&proc->mtx);
	InitializeConditionVariable(&proc->queue_cond);
	InitializeConditionVariable(&proc->done_cond);

	if (data_writer_init(proc, max_block_size, cmp, num_workers,
			     max_backlog, devblksz, file)) {
		goto fail;
	}

	for (i = 0; i < num_workers; ++i) {
		proc->workers[i] = alloc_flex(sizeof(compress_worker_t),
					      1, max_block_size);

		if (proc->workers[i] == NULL)
			goto fail;

		proc->workers[i]->shared = proc;
		proc->workers[i]->cmp = cmp->create_copy(cmp);

		if (proc->workers[i]->cmp == NULL)
			goto fail;

		proc->workers[i]->thread = CreateThread(NULL, 0, worker_proc,
							proc->workers[i], 0, 0);
		if (proc->workers[i]->thread == NULL)
			goto fail;
	}

	return proc;
fail:
	sqfs_data_writer_destroy(proc);
	return NULL;
}

void sqfs_data_writer_destroy(sqfs_data_writer_t *proc)
{
	unsigned int i;

	EnterCriticalSection(&proc->mtx);
	proc->status = -1;
	WakeAllConditionVariable(&proc->queue_cond);
	LeaveCriticalSection(&proc->mtx);

	for (i = 0; i < proc->num_workers; ++i) {
		if (proc->workers[i] == NULL)
			continue;

		if (proc->workers[i]->thread != NULL) {
			WaitForSingleObject(proc->workers[i]->thread, INFINITE);
			CloseHandle(proc->workers[i]->thread);
		}

		if (proc->workers[i]->cmp != NULL)
			proc->workers[i]->cmp->destroy(proc->workers[i]->cmp);

		free(proc->workers[i]);
	}

	DeleteCriticalSection(&proc->mtx);
	data_writer_cleanup(proc);
}

static void append_to_work_queue(sqfs_data_writer_t *proc,
				 sqfs_block_t *block)
{
	if (proc->queue_last == NULL) {
		proc->queue = proc->queue_last = block;
	} else {
		proc->queue_last->next = block;
		proc->queue_last = block;
	}

	block->sequence_number = proc->enqueue_id++;
	block->next = NULL;
	proc->backlog += 1;
	WakeAllConditionVariable(&proc->queue_cond);
}

static sqfs_block_t *try_dequeue(sqfs_data_writer_t *proc)
{
	sqfs_block_t *queue, *it, *prev;

	it = proc->done;
	prev = NULL;

	while (it != NULL && it->sequence_number == proc->dequeue_id) {
		prev = it;
		it = it->next;
		proc->dequeue_id += 1;
	}

	if (prev == NULL) {
		queue = NULL;
	} else {
		queue = proc->done;
		prev->next = NULL;
		proc->done = it;
	}

	return queue;
}

static sqfs_block_t *queue_merge(sqfs_block_t *lhs, sqfs_block_t *rhs)
{
	sqfs_block_t *it, *head = NULL, **next_ptr = &head;

	while (lhs != NULL && rhs != NULL) {
		if (lhs->sequence_number <= rhs->sequence_number) {
			it = lhs;
			lhs = lhs->next;
		} else {
			it = rhs;
			rhs = rhs->next;
		}

		*next_ptr = it;
		next_ptr = &it->next;
	}

	it = (lhs != NULL ? lhs : rhs);
	*next_ptr = it;
	return head;
}

static int process_done_queue(sqfs_data_writer_t *proc, sqfs_block_t *queue)
{
	sqfs_block_t *it, *block = NULL;
	int status = 0;

	while (queue != NULL && status == 0) {
		it = queue;
		queue = it->next;

		if (it->flags & SQFS_BLK_IS_FRAGMENT) {
			block = NULL;
			status = process_completed_fragment(proc, it, &block);

			if (block != NULL && status == 0) {
				EnterCriticalSection(&proc->mtx);
				proc->dequeue_id = it->sequence_number;
				block->sequence_number = it->sequence_number;

				if (proc->queue == NULL) {
					proc->queue = block;
					proc->queue_last = block;
				} else {
					block->next = proc->queue;
					proc->queue = block;
				}

				proc->backlog += 1;
				proc->done = queue_merge(queue, proc->done);
				WakeAllConditionVariable(&proc->queue_cond);
				LeaveCriticalSection(&proc->mtx);

				queue = NULL;
			} else {
				free(block);
			}
		} else {
			status = process_completed_block(proc, it);
		}

		free(it);
	}

	free_blk_list(queue);
	return status;
}

int test_and_set_status(sqfs_data_writer_t *proc, int status)
{
	EnterCriticalSection(&proc->mtx);
	if (proc->status == 0) {
		proc->status = status;
	} else {
		status = proc->status;
	}
	WakeAllConditionVariable(&proc->queue_cond);
	LeaveCriticalSection(&proc->mtx);
	return status;
}

int data_writer_enqueue(sqfs_data_writer_t *proc, sqfs_block_t *block)
{
	sqfs_block_t *queue;
	int status;

	EnterCriticalSection(&proc->mtx);
	while (proc->backlog > proc->max_backlog && proc->status == 0) {
		SleepConditionVariableCS(&proc->done_cond, &proc->mtx,
					 INFINITE);
	}

	if (proc->status != 0) {
		status = proc->status;
		LeaveCriticalSection(&proc->mtx);
		free(block);
		return status;
	}

	append_to_work_queue(proc, block);
	block = NULL;

	queue = try_dequeue(proc);
	LeaveCriticalSection(&proc->mtx);

	status = process_done_queue(proc, queue);

	return status ? test_and_set_status(proc, status) : status;
}

int sqfs_data_writer_finish(sqfs_data_writer_t *proc)
{
	sqfs_block_t *queue;
	int status = 0;

	for (;;) {
		EnterCriticalSection(&proc->mtx);
		while (proc->backlog > 0 && proc->status == 0) {
			SleepConditionVariableCS(&proc->done_cond, &proc->mtx,
						 INFINITE);
		}

		if (proc->status != 0) {
			status = proc->status;
			LeaveCriticalSection(&proc->mtx);
			return status;
		}

		queue = proc->done;
		proc->done = NULL;
		LeaveCriticalSection(&proc->mtx);

		if (queue == NULL) {
			if (proc->frag_block != NULL) {
				append_to_work_queue(proc, proc->frag_block);
				proc->frag_block = NULL;
				continue;
			}
			break;
		}

		status = process_done_queue(proc, queue);
		if (status != 0)
			return status;
	}

	return 0;
}
