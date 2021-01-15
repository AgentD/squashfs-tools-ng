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
#	define LOCK EnterCriticalSection
#	define UNLOCK LeaveCriticalSection
#	define AWAIT(cond, mtx) SleepConditionVariableCS(cond, mtx, INFINITE)
#	define SIGNAL_ALL WakeAllConditionVariable
#	define THREAD_CREATE(hndptr, proc, arg) \
		  ((*hndptr) = CreateThread(NULL, 0, proc, arg, 0, 0), \
		   ((*hndptr) == NULL) ? -1 : 0)
#	define THREAD_JOIN(t) \
		do { \
			WaitForSingleObject(t, INFINITE); \
			CloseHandle(t); \
		} while (0)
#	define MUTEX_INIT InitializeCriticalSection
#	define MUTEX_DESTROY DeleteCriticalSection
#	define CONDITION_INIT InitializeConditionVariable
#	define CONDITION_DESTROY(cond)
#	define THREAD_EXIT_SUCCESS 0
#	define THREAD_INVALID NULL
#	define THREAD_TYPE DWORD WINAPI
#	define SIGNAL_DISABLE(oldset) (void)oldset
#	define SIGNAL_ENABLE(oldset) (void)oldset

typedef int sigset_t;
typedef LPVOID THREAD_ARG;
typedef HANDLE THREAD_HANDLE;
typedef CRITICAL_SECTION MUTEX_TYPE;
typedef CONDITION_VARIABLE CONDITION_TYPE;
#else
#	include <pthread.h>
#	include <signal.h>
#	define LOCK pthread_mutex_lock
#	define UNLOCK pthread_mutex_unlock
#	define AWAIT pthread_cond_wait
#	define SIGNAL_ALL pthread_cond_broadcast
#	define THREAD_CREATE(hndptr, proc, arg) \
		  pthread_create((hndptr), NULL, (proc), (arg))
#	define THREAD_JOIN(t) pthread_join((t), NULL)
#	define MUTEX_INIT(mtx) \
		*(mtx) = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER
#	define MUTEX_DESTROY pthread_mutex_destroy
#	define CONDITION_INIT(cond) \
		*(cond) = (pthread_cond_t)PTHREAD_COND_INITIALIZER
#	define CONDITION_DESTROY pthread_cond_destroy
#	define THREAD_EXIT_SUCCESS NULL
#	define THREAD_INVALID (pthread_t)0
#	define SIGNAL_ENABLE(oldset) pthread_sigmask(SIG_SETMASK, oldset, NULL)

typedef void *THREAD_TYPE;
typedef void *THREAD_ARG;
typedef pthread_t THREAD_HANDLE;
typedef pthread_mutex_t MUTEX_TYPE;
typedef pthread_cond_t CONDITION_TYPE;

static inline void SIGNAL_DISABLE(sigset_t *oldset)
{
	sigset_t set;
	sigfillset(&set);
	pthread_sigmask(SIG_SETMASK, &set, oldset);
}
#endif

typedef struct compress_worker_t compress_worker_t;
typedef struct thread_pool_processor_t thread_pool_processor_t;

struct compress_worker_t {
	thread_pool_processor_t *shared;
	sqfs_compressor_t *cmp;
	THREAD_HANDLE thread;
	sqfs_u32 frag_blk_index;
	sqfs_u32 frag_blk_size;
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
	sqfs_block_processor_t *proc = (sqfs_block_processor_t *)shared;
	sqfs_block_t *blk = NULL;
	int status = 0;

	for (;;) {
		LOCK(&shared->mtx);
		if (blk != NULL)
			store_completed_block(shared, blk, status);

		blk = get_next_work_item(shared);

		if (blk != NULL && (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) &&
		    shared->base.uncmp != NULL && shared->base.file != NULL) {
			memcpy(worker->scratch + shared->base.max_block_size,
			       blk->data, blk->size);
			worker->frag_blk_index = blk->index;
			worker->frag_blk_size = blk->size;
		}
		UNLOCK(&shared->mtx);

		if (blk == NULL)
			break;

		status = proc->process_block(blk, worker->cmp, worker->scratch,
					     proc->max_block_size);
	}

	return THREAD_EXIT_SUCCESS;
}

static void block_processor_destroy(sqfs_object_t *obj)
{
	thread_pool_processor_t *proc = (thread_pool_processor_t *)obj;
	unsigned int i;

	LOCK(&proc->mtx);
	proc->status = -1;
	SIGNAL_ALL(&proc->queue_cond);
	UNLOCK(&proc->mtx);

	for (i = 0; i < proc->num_workers; ++i) {
		if (proc->workers[i] != NULL) {
			if (proc->workers[i]->thread != THREAD_INVALID)
				THREAD_JOIN(proc->workers[i]->thread);

			if (proc->workers[i]->cmp != NULL)
				sqfs_destroy(proc->workers[i]->cmp);

			free(proc->workers[i]);
		}
	}

	CONDITION_DESTROY(&proc->done_cond);
	CONDITION_DESTROY(&proc->queue_cond);
	MUTEX_DESTROY(&proc->mtx);

	free_blk_list(proc->proc_queue);
	free_blk_list(proc->io_queue);
	free_blk_list(proc->done);
	block_processor_cleanup(&proc->base);
	free(proc);
}

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
	sqfs_block_t *it;
	int status = 0;

	while (status == 0 && list != NULL) {
		it = list;
		list = list->next;
		status = proc->base.process_completed_block(&proc->base, it);

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

static int append_to_work_queue(sqfs_block_processor_t *proc,
				sqfs_block_t *block)
{
	thread_pool_processor_t *thproc = (thread_pool_processor_t *)proc;
	sqfs_block_t *io_list = NULL, *io_list_last = NULL;
	sqfs_block_t *blk, *fragblk;
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
			thproc->status =
				proc->process_completed_fragment(proc, blk,
								 &fragblk);

			if (fragblk != NULL) {
				fragblk->io_seq_num = thproc->io_enq_id++;
				append_block(thproc, fragblk);
				SIGNAL_ALL(&thproc->queue_cond);
			}
		} else {
			if (!(blk->flags & SQFS_BLK_FRAGMENT_BLOCK) ||
			    blk->flags & BLK_FLAG_MANUAL_SUBMISSION)
				blk->io_seq_num = thproc->io_enq_id++;
			store_io_block(thproc, blk);
		}
	}
	SIGNAL_ALL(&thproc->queue_cond);
	UNLOCK(&thproc->mtx);
	free(block);

	if (status == 0) {
		status = handle_io_queue(thproc, io_list);
	} else {
		free_blk_list(io_list);
	}

	return status;
}

static sqfs_block_t *find_frag_blk_in_queue(sqfs_block_t *q, sqfs_u32 index)
{
	while (q != NULL) {
		if ((q->flags & SQFS_BLK_FRAGMENT_BLOCK) && q->index == index)
			break;
		q = q->next;
	}
	return q;
}

static int compare_frag_in_flight(sqfs_block_processor_t *proc,
				  sqfs_block_t *frag, sqfs_u32 index,
				  sqfs_u32 offset)
{
	thread_pool_processor_t *thproc = (thread_pool_processor_t *)proc;
	sqfs_block_t *it = NULL;
	void *blockbuf = NULL;
	size_t i, size = 0;
	int ret;

	if (proc->frag_block != NULL && proc->frag_block->index == index)
		it = proc->frag_block;

	if (it == NULL)
		it = find_frag_blk_in_queue(thproc->proc_queue, index);

	if (it == NULL)
		it = find_frag_blk_in_queue(thproc->io_queue, index);

	if (it == NULL)
		it = find_frag_blk_in_queue(thproc->done, index);

	if (it == NULL) {
		for (i = 0; i < thproc->num_workers; ++i) {
			if (thproc->workers[i]->frag_blk_index == index) {
				size = thproc->workers[i]->frag_blk_size;
				blockbuf = thproc->workers[i]->scratch +
					proc->max_block_size;
				break;
			}
		}
	} else if (it->flags & SQFS_BLK_IS_COMPRESSED) {
		proc->buffered_index = 0xFFFFFFFF;
		blockbuf = proc->frag_buffer;
		ret = proc->uncmp->do_block(proc->uncmp, it->data, it->size,
					    blockbuf, proc->max_block_size);
		if (ret <= 0)
			return -1;
		proc->buffered_index = it->index;
		size = ret;
	} else {
		blockbuf = it->data;
		size = it->size;
	}

	if (blockbuf == NULL || size == 0)
		return -1;

	if (offset >= size || frag->size > (size - offset))
		return -1;

	return memcmp((const char *)blockbuf + offset, frag->data, frag->size);
}

static int block_processor_sync(sqfs_block_processor_t *proc)
{
	return append_to_work_queue(proc, NULL);
}

int sqfs_block_processor_create_ex(const sqfs_block_processor_desc_t *desc,
				   sqfs_block_processor_t **out)
{
	thread_pool_processor_t *proc;
	unsigned int i, num_workers;
	size_t scratch_size;
	sigset_t oldset;
	int ret;

	if (desc->size != sizeof(sqfs_block_processor_desc_t))
		return SQFS_ERROR_ARG_INVALID;

	num_workers = desc->num_workers < 1 ? 1 : desc->num_workers;

	proc = alloc_flex(sizeof(*proc),
			  sizeof(proc->workers[0]), num_workers);
	if (proc == NULL)
		return SQFS_ERROR_ALLOC;

	ret = block_processor_init(&proc->base, desc);
	if (ret != 0) {
		free(proc);
		return ret;
	}

	proc->base.sync = block_processor_sync;
	proc->base.append_to_work_queue = append_to_work_queue;
	proc->base.compare_frag_in_flight = compare_frag_in_flight;
	proc->num_workers = num_workers;
	proc->max_backlog = desc->max_backlog;
	((sqfs_object_t *)proc)->destroy = block_processor_destroy;

	MUTEX_INIT(&proc->mtx);
	CONDITION_INIT(&proc->queue_cond);
	CONDITION_INIT(&proc->done_cond);

	SIGNAL_DISABLE(&oldset);

	for (i = 0; i < num_workers; ++i) {
		scratch_size = desc->max_block_size;
		if (desc->uncmp != NULL && desc->file != NULL)
			scratch_size *= 2;

		proc->workers[i] = alloc_flex(sizeof(compress_worker_t),
					      1, scratch_size);

		if (proc->workers[i] == NULL) {
			ret = SQFS_ERROR_ALLOC;
			goto fail;
		}

		proc->workers[i]->shared = proc;
		proc->workers[i]->cmp = sqfs_copy(desc->cmp);
		proc->workers[i]->frag_blk_index = 0xFFFFFFFF;

		if (proc->workers[i]->cmp == NULL) {
			ret = SQFS_ERROR_ALLOC;
			goto fail;
		}

		ret = THREAD_CREATE(&proc->workers[i]->thread,
				    worker_proc, proc->workers[i]);
		if (ret != 0) {
			ret = SQFS_ERROR_INTERNAL;
			goto fail;
		}
	}

	SIGNAL_ENABLE(&oldset);
	*out = (sqfs_block_processor_t *)proc;
	return 0;
fail:
	SIGNAL_ENABLE(&oldset);
	block_processor_destroy((sqfs_object_t *)proc);
	return ret;
}
