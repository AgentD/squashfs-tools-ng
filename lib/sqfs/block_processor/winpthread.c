/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * winpthread.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

#if defined(_WIN32) || defined(__WINDOWS__)
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	define LOCK(mtx) EnterCriticalSection(mtx)
#	define UNLOCK(mtx) LeaveCriticalSection(mtx)
#	define AWAIT(cond, mtx) SleepConditionVariableCS(cond, mtx, INFINITE)
#	define SIGNAL_ALL(cond) WakeAllConditionVariable(cond)
#	define THREAD_EXIT_SUCCESS 0
#	define THREAD_TYPE DWORD WINAPI
#	define THREAD_ARG LPVOID
#	define THREAD_HANDLE HANDLE
#	define MUTEX_TYPE CRITICAL_SECTION
#	define CONDITION_TYPE CONDITION_VARIABLE
#else
#	include <pthread.h>
#	include <signal.h>
#	define LOCK(mtx) pthread_mutex_lock(mtx)
#	define UNLOCK(mtx) pthread_mutex_unlock(mtx)
#	define AWAIT(cond, mtx) pthread_cond_wait(cond, mtx)
#	define SIGNAL_ALL(cond) pthread_cond_broadcast(cond)
#	define THREAD_EXIT_SUCCESS NULL
#	define THREAD_TYPE void *
#	define THREAD_ARG void *
#	define THREAD_HANDLE pthread_t
#	define MUTEX_TYPE pthread_mutex_t
#	define CONDITION_TYPE pthread_cond_t
#endif

typedef struct compress_worker_t compress_worker_t;
typedef struct thread_pool_processor_t thread_pool_processor_t;

struct compress_worker_t {
	thread_pool_processor_t *shared;
	sqfs_compressor_t *cmp;
	THREAD_HANDLE thread;
	sqfs_u8 scratch[];
};

struct thread_pool_processor_t {
	sqfs_block_processor_t base;

	MUTEX_TYPE mtx;
	CONDITION_TYPE queue_cond;
	CONDITION_TYPE done_cond;

	sqfs_block_t *proc_queue;
	sqfs_block_t *proc_queue_last;

	sqfs_block_t *io_queue;
	sqfs_block_t *done;
	size_t backlog;
	int status;

	sqfs_u32 proc_enq_id;
	sqfs_u32 proc_deq_id;

	sqfs_u32 io_enq_id;
	sqfs_u32 io_deq_id;

	unsigned int num_workers;
	size_t max_backlog;

	compress_worker_t *workers[];
};

static void free_blk_list(sqfs_block_t *list)
{
	sqfs_block_t *it;

	while (list != NULL) {
		it = list;
		list = list->next;
		free(it);
	}
}

static sqfs_block_t *get_next_work_item(thread_pool_processor_t *shared)
{
	sqfs_block_t *blk = NULL;

	while (shared->proc_queue == NULL && shared->status == 0)
		AWAIT(&shared->queue_cond, &shared->mtx);

	if (shared->status == 0) {
		blk = shared->proc_queue;
		shared->proc_queue = blk->next;
		blk->next = NULL;

		if (shared->proc_queue == NULL)
			shared->proc_queue_last = NULL;
	}

	return blk;
}

static void store_completed_block(thread_pool_processor_t *shared,
				  sqfs_block_t *blk, int status)
{
	sqfs_block_t *it = shared->done, *prev = NULL;

	while (it != NULL) {
		if (it->proc_seq_num >= blk->proc_seq_num)
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

static THREAD_TYPE worker_proc(THREAD_ARG arg)
{
	compress_worker_t *worker = arg;
	thread_pool_processor_t *shared = worker->shared;
	sqfs_block_t *blk = NULL;
	int status = 0;

	for (;;) {
		LOCK(&shared->mtx);
		if (blk != NULL)
			store_completed_block(shared, blk, status);

		blk = get_next_work_item(shared);
		UNLOCK(&shared->mtx);

		if (blk == NULL)
			break;

		status = block_processor_do_block(blk, worker->cmp,
						  worker->scratch,
						  shared->base.max_block_size);
	}

	return THREAD_EXIT_SUCCESS;
}

#if defined(_WIN32) || defined(__WINDOWS__)
static void block_processor_destroy(sqfs_object_t *obj)
{
	thread_pool_processor_t *proc = (thread_pool_processor_t *)obj;
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
	free_blk_list(proc->proc_queue);
	free_blk_list(proc->done);
	free(proc->base.blk_current);
	free(proc->base.frag_block);
	free(proc);
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	thread_pool_processor_t *proc;
	unsigned int i;

	if (num_workers < 1)
		num_workers = 1;

	proc = alloc_flex(sizeof(*proc),
			  sizeof(proc->workers[0]), num_workers);
	if (proc == NULL)
		return NULL;

	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->base.max_block_size = max_block_size;
	proc->base.cmp = cmp;
	proc->base.frag_tbl = tbl;
	proc->base.wr = wr;
	proc->base.stats.size = sizeof(proc->base.stats);
	((sqfs_object_t *)obj)->destroy = block_processor_destroy;

	InitializeCriticalSection(&proc->mtx);
	InitializeConditionVariable(&proc->queue_cond);
	InitializeConditionVariable(&proc->done_cond);

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

	return (sqfs_block_processor_t *)proc;
fail:
	block_processor_destroy(proc);
	return NULL;
}
#else
static void block_processor_destroy(sqfs_object_t *obj)
{
	thread_pool_processor_t *proc = (thread_pool_processor_t *)obj;
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

	free_blk_list(proc->proc_queue);
	free_blk_list(proc->done);
	free(proc->base.blk_current);
	free(proc->base.frag_block);
	free(proc);
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	thread_pool_processor_t *proc;
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
	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->base.max_block_size = max_block_size;
	proc->base.cmp = cmp;
	proc->base.frag_tbl = tbl;
	proc->base.wr = wr;
	proc->base.stats.size = sizeof(proc->base.stats);

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

	return (sqfs_block_processor_t *)proc;
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
	free(proc);
	return NULL;
}
#endif

static void store_io_block(thread_pool_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_block_t *it = proc->io_queue, *prev = NULL;

	while (it != NULL && it->io_seq_num < blk->io_seq_num) {
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
}

static sqfs_block_t *try_dequeue_io(thread_pool_processor_t *proc)
{
	sqfs_block_t *out;

	if (proc->io_queue == NULL)
		return NULL;

	if (proc->io_queue->io_seq_num != proc->io_deq_id)
		return NULL;

	out = proc->io_queue;
	proc->io_queue = out->next;
	out->next = NULL;
	proc->io_deq_id += 1;
	proc->backlog -= 1;
	return out;
}

static sqfs_block_t *try_dequeue_done(thread_pool_processor_t *proc)
{
	sqfs_block_t *out;

	if (proc->done == NULL)
		return NULL;

	if (proc->done->proc_seq_num != proc->proc_deq_id)
		return NULL;

	out = proc->done;
	proc->done = out->next;
	out->next = NULL;
	proc->proc_deq_id += 1;
	proc->backlog -= 1;
	return out;
}

static void append_block(thread_pool_processor_t *proc, sqfs_block_t *block)
{
	if (proc->proc_queue_last == NULL) {
		proc->proc_queue = proc->proc_queue_last = block;
	} else {
		proc->proc_queue_last->next = block;
		proc->proc_queue_last = block;
	}

	block->proc_seq_num = proc->proc_enq_id++;
	block->next = NULL;
	proc->backlog += 1;
}

static int handle_io_queue(thread_pool_processor_t *proc, sqfs_block_t *list)
{
	sqfs_block_t *it = list;
	int status = 0;

	while (status == 0 && it != NULL) {
		status = process_completed_block(&proc->base, it);
		it = it->next;

		if (status != 0) {
			LOCK(&proc->mtx);
			if (proc->status == 0)
				proc->status = status;
			SIGNAL_ALL(&proc->queue_cond);
			UNLOCK(&proc->mtx);
		}
	}

	return status;
}

int append_to_work_queue(sqfs_block_processor_t *proc, sqfs_block_t *block)
{
	thread_pool_processor_t *thproc = (thread_pool_processor_t *)proc;
	sqfs_block_t *io_list = NULL, *io_list_last = NULL;
	sqfs_block_t *blk, *fragblk, *free_list = NULL;
	int status;

	LOCK(&thproc->mtx);
	for (;;) {
		status = thproc->status;
		if (status != 0)
			break;

		if (block == NULL) {
			if (thproc->backlog == 0)
				break;
		} else {
			if (thproc->backlog < thproc->max_backlog) {
				append_block(thproc, block);
				block = NULL;
				break;
			}
		}

		blk = try_dequeue_io(thproc);
		if (blk != NULL) {
			if (io_list_last == NULL) {
				io_list = io_list_last = blk;
			} else {
				io_list_last->next = blk;
				io_list_last = blk;
			}
			continue;
		}

		blk = try_dequeue_done(thproc);
		if (blk == NULL) {
			AWAIT(&thproc->done_cond, &thproc->mtx);
			continue;
		}

		if (blk->flags & SQFS_BLK_IS_FRAGMENT) {
			fragblk = NULL;
			thproc->status = process_completed_fragment(proc, blk,
								    &fragblk);
			blk->next = free_list;
			free_list = blk;

			if (fragblk != NULL) {
				fragblk->io_seq_num = thproc->io_enq_id++;
				append_block(thproc, fragblk);
				SIGNAL_ALL(&thproc->queue_cond);
			}
		} else {
			if (!(blk->flags & SQFS_BLK_FRAGMENT_BLOCK))
				blk->io_seq_num = thproc->io_enq_id++;
			store_io_block(thproc, blk);
		}
	}
	SIGNAL_ALL(&thproc->queue_cond);
	UNLOCK(&thproc->mtx);
	free(block);

	if (status == 0)
		status = handle_io_queue(thproc, io_list);

	free_blk_list(io_list);
	free_blk_list(free_list);
	return status;
}

int sqfs_block_processor_sync(sqfs_block_processor_t *proc)
{
	return append_to_work_queue(proc, NULL);
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	thread_pool_processor_t *thproc = (thread_pool_processor_t *)proc;
	sqfs_block_t *blk;
	int status;

	status = append_to_work_queue(proc, NULL);

	if (status == 0 && proc->frag_block != NULL) {
		blk = proc->frag_block;
		blk->next = NULL;
		proc->frag_block = NULL;

		status = block_processor_do_block(blk, proc->cmp,
						  thproc->workers[0]->scratch,
						  proc->max_block_size);

		if (status == 0)
			status = handle_io_queue(thproc, blk);
		free(blk);

		if (status != 0) {
			LOCK(&thproc->mtx);
			if (thproc->status == 0)
				thproc->status = status;
			SIGNAL_ALL(&thproc->queue_cond);
			UNLOCK(&thproc->mtx);
		}
	}

	return status;
}
