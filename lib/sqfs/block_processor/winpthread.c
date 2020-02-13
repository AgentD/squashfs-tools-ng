/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * winpthread.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

#if defined(_WIN32) || defined(__WINDOWS__)
#	define LOCK(mtx) EnterCriticalSection(mtx)
#	define UNLOCK(mtx) LeaveCriticalSection(mtx)
#	define AWAIT(cond, mtx) SleepConditionVariableCS(cond, mtx, INFINITE)
#	define SIGNAL_ALL(cond) WakeAllConditionVariable(cond)
#	define THREAD_EXIT_SUCCESS 0
#	define THREAD_TYPE DWORD WINAPI
#	define THREAD_ARG LPVOID
#	define THREAD_HANDLE HANDLE
#else
#	define LOCK(mtx) pthread_mutex_lock(mtx)
#	define UNLOCK(mtx) pthread_mutex_unlock(mtx)
#	define AWAIT(cond, mtx) pthread_cond_wait(cond, mtx)
#	define SIGNAL_ALL(cond) pthread_cond_broadcast(cond)
#	define THREAD_EXIT_SUCCESS NULL
#	define THREAD_TYPE void *
#	define THREAD_ARG void *
#	define THREAD_HANDLE pthread_t
#endif

struct compress_worker_t {
	sqfs_block_processor_t *shared;
	sqfs_compressor_t *cmp;
	THREAD_HANDLE thread;
	sqfs_u8 scratch[];
};

static THREAD_TYPE worker_proc(THREAD_ARG arg)
{
	compress_worker_t *worker = arg;
	sqfs_block_processor_t *shared = worker->shared;
	sqfs_block_t *it, *prev, *blk = NULL;
	int status = 0;

	for (;;) {
		LOCK(&shared->mtx);
		if (blk != NULL) {
			it = shared->done;
			prev = NULL;

			while (it != NULL) {
				if (it->sequence_number >= blk->sequence_number)
					break;
				prev = it;
				it = it->next;
			}

			if (prev == NULL) {
				blk->next = shared->done;
				shared->done = blk;
			} else {
				blk->next = prev->next;
				prev->next = blk;
			}

			if (status != 0 && shared->status == 0)
				shared->status = status;
			SIGNAL_ALL(&shared->done_cond);
		}

		while (shared->queue == NULL && shared->status == 0)
			AWAIT(&shared->queue_cond, &shared->mtx);

		if (shared->status == 0) {
			blk = shared->queue;
			shared->queue = blk->next;
			blk->next = NULL;

			if (shared->queue == NULL)
				shared->queue_last = NULL;
		} else {
			blk = NULL;
		}
		UNLOCK(&shared->mtx);

		if (blk == NULL)
			break;

		status = block_processor_do_block(blk, worker->cmp,
						  worker->scratch,
						  shared->max_block_size);
	}

	return THREAD_EXIT_SUCCESS;
}

#if defined(_WIN32) || defined(__WINDOWS__)
static void block_processor_destroy(sqfs_object_t *obj)
{
	sqfs_block_processor_t *proc = (sqfs_block_processor_t *)obj;
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
			sqfs_destroy(proc->workers[i]->cmp);

		free(proc->workers[i]);
	}

	DeleteCriticalSection(&proc->mtx);
	block_processor_cleanup(proc);
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	sqfs_block_processor_t *proc;
	unsigned int i;

	if (num_workers < 1)
		num_workers = 1;

	proc = alloc_flex(sizeof(*proc),
			  sizeof(proc->workers[0]), num_workers);
	if (proc == NULL)
		return NULL;

	((sqfs_object_t *)obj)->destroy = block_processor_destroy;

	InitializeCriticalSection(&proc->mtx);
	InitializeConditionVariable(&proc->queue_cond);
	InitializeConditionVariable(&proc->done_cond);

	if (block_processor_init(proc, max_block_size, cmp, num_workers,
				 max_backlog, wr, tbl)) {
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
	block_processor_destroy(proc);
	return NULL;
}
#else
static void block_processor_destroy(sqfs_object_t *obj)
{
	sqfs_block_processor_t *proc = (sqfs_block_processor_t *)obj;
	unsigned int i;

	pthread_mutex_lock(&proc->mtx);
	proc->status = -1;
	pthread_cond_broadcast(&proc->queue_cond);
	pthread_mutex_unlock(&proc->mtx);

	for (i = 0; i < proc->num_workers; ++i) {
		pthread_join(proc->workers[i]->thread, NULL);

		sqfs_destroy(proc->workers[i]->cmp);
		free(proc->workers[i]);
	}

	pthread_cond_destroy(&proc->done_cond);
	pthread_cond_destroy(&proc->queue_cond);
	pthread_mutex_destroy(&proc->mtx);

	block_processor_cleanup(proc);
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	sqfs_block_processor_t *proc;
	sigset_t set, oldset;
	unsigned int i;
	int ret;

	if (num_workers < 1)
		num_workers = 1;

	proc = alloc_flex(sizeof(*proc),
			  sizeof(proc->workers[0]), num_workers);
	if (proc == NULL)
		return NULL;

	((sqfs_object_t *)proc)->destroy = block_processor_destroy;
	proc->mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	proc->queue_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	proc->done_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

	if (block_processor_init(proc, max_block_size, cmp, num_workers,
				 max_backlog, wr, tbl)) {
		goto fail_init;
	}

	for (i = 0; i < num_workers; ++i) {
		proc->workers[i] = alloc_flex(sizeof(compress_worker_t),
					      1, max_block_size);

		if (proc->workers[i] == NULL)
			goto fail_init;

		proc->workers[i]->shared = proc;
		proc->workers[i]->cmp = cmp->create_copy(cmp);

		if (proc->workers[i]->cmp == NULL)
			goto fail_init;
	}

	sigfillset(&set);
	pthread_sigmask(SIG_SETMASK, &set, &oldset);

	for (i = 0; i < num_workers; ++i) {
		ret = pthread_create(&proc->workers[i]->thread, NULL,
				     worker_proc, proc->workers[i]);

		if (ret != 0)
			goto fail_thread;
	}

	pthread_sigmask(SIG_SETMASK, &oldset, NULL);

	return proc;
fail_thread:
	pthread_mutex_lock(&proc->mtx);
	proc->status = -1;
	pthread_cond_broadcast(&proc->queue_cond);
	pthread_mutex_unlock(&proc->mtx);

	for (i = 0; i < num_workers; ++i) {
		if (proc->workers[i]->thread > 0) {
			pthread_join(proc->workers[i]->thread, NULL);
		}
	}
	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
fail_init:
	for (i = 0; i < num_workers; ++i) {
		if (proc->workers[i] != NULL) {
			if (proc->workers[i]->cmp != NULL)
				sqfs_destroy(proc->workers[i]->cmp);

			free(proc->workers[i]);
		}
	}
	pthread_cond_destroy(&proc->done_cond);
	pthread_cond_destroy(&proc->queue_cond);
	pthread_mutex_destroy(&proc->mtx);
	block_processor_cleanup(proc);
	return NULL;
}
#endif

int append_to_work_queue(sqfs_block_processor_t *proc, sqfs_block_t *block)
{
	int status;

	LOCK(&proc->mtx);
	status = proc->status;
	if (status != 0)
		goto out;

	if (block != NULL) {
		if (proc->queue_last == NULL) {
			proc->queue = proc->queue_last = block;
		} else {
			proc->queue_last->next = block;
			proc->queue_last = block;
		}

		block->sequence_number = proc->enqueue_id++;
		block->next = NULL;
		proc->backlog += 1;
		block = NULL;
	}
out:
	SIGNAL_ALL(&proc->queue_cond);
	UNLOCK(&proc->mtx);
	free(block);
	return 0;
}

static sqfs_block_t *try_dequeue(sqfs_block_processor_t *proc)
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

static int process_done_queue(sqfs_block_processor_t *proc, sqfs_block_t *queue)
{
	sqfs_block_t *it, *block = NULL;
	int status = 0;

	while (queue != NULL && status == 0) {
		it = queue;
		queue = it->next;
		proc->backlog -= 1;

		if (it->flags & SQFS_BLK_IS_FRAGMENT) {
			block = NULL;
			status = process_completed_fragment(proc, it, &block);

			if (block != NULL && status == 0) {
				LOCK(&proc->mtx);
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
				SIGNAL_ALL(&proc->queue_cond);
				UNLOCK(&proc->mtx);

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

int wait_completed(sqfs_block_processor_t *proc)
{
	sqfs_block_t *queue;
	int status;

	LOCK(&proc->mtx);
	for (;;) {
		queue = try_dequeue(proc);
		status = proc->status;

		if (queue != NULL || status != 0)
			break;

		AWAIT(&proc->done_cond, &proc->mtx);
	}
	UNLOCK(&proc->mtx);

	if (status != 0) {
		free_blk_list(queue);
		return status;
	}

	status = process_done_queue(proc, queue);

	if (status != 0) {
		LOCK(&proc->mtx);
		if (proc->status == 0) {
			proc->status = status;
		} else {
			status = proc->status;
		}
		SIGNAL_ALL(&proc->queue_cond);
		UNLOCK(&proc->mtx);
	}
	return status;
}
