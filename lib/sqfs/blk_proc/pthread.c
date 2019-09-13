/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>

typedef struct {
	sqfs_block_processor_t *shared;
	sqfs_compressor_t *cmp;
	pthread_t thread;
	uint8_t scratch[];
} compress_worker_t;

struct sqfs_block_processor_t {
	pthread_mutex_t mtx;
	pthread_cond_t queue_cond;
	pthread_cond_t done_cond;

	/* needs rw access by worker and main thread */
	sqfs_block_t *queue;
	sqfs_block_t *queue_last;

	sqfs_block_t *done;
	bool terminate;
	size_t backlog;

	/* used by main thread only */
	uint32_t enqueue_id;
	uint32_t dequeue_id;

	unsigned int num_workers;
	sqfs_block_cb cb;
	void *user;
	int status;
	size_t max_backlog;

	/* used only by workers */
	size_t max_block_size;

	compress_worker_t *workers[];
};

static void store_completed_block(sqfs_block_processor_t *shared,
				  sqfs_block_t *blk)
{
	sqfs_block_t *it = shared->done, *prev = NULL;

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
}

static void *worker_proc(void *arg)
{
	compress_worker_t *worker = arg;
	sqfs_block_processor_t *shared = worker->shared;
	sqfs_block_t *blk = NULL;

	for (;;) {
		pthread_mutex_lock(&shared->mtx);
		if (blk != NULL) {
			store_completed_block(shared, blk);
			shared->backlog -= 1;
			pthread_cond_broadcast(&shared->done_cond);
		}

		while (shared->queue == NULL && !shared->terminate) {
			pthread_cond_wait(&shared->queue_cond,
					  &shared->mtx);
		}

		if (shared->terminate) {
			pthread_mutex_unlock(&shared->mtx);
			break;
		}

		blk = shared->queue;
		shared->queue = blk->next;
		blk->next = NULL;

		if (shared->queue == NULL)
			shared->queue_last = NULL;
		pthread_mutex_unlock(&shared->mtx);

		sqfs_block_process(blk, worker->cmp, worker->scratch,
				   shared->max_block_size);
	}
	return NULL;
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    void *user,
						    sqfs_block_cb callback)
{
	sqfs_block_processor_t *proc;
	unsigned int i;
	int ret;

	if (num_workers < 1)
		num_workers = 1;

	proc = alloc_flex(sizeof(*proc),
			  sizeof(proc->workers[0]), num_workers);
	if (proc == NULL)
		return NULL;

	proc->max_block_size = max_block_size;
	proc->cb = callback;
	proc->user = user;
	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	proc->queue_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	proc->done_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

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

	for (i = 0; i < num_workers; ++i) {
		ret = pthread_create(&proc->workers[i]->thread, NULL,
				     worker_proc, proc->workers[i]);

		if (ret != 0)
			goto fail_thread;
	}

	return proc;
fail_thread:
	pthread_mutex_lock(&proc->mtx);
	proc->terminate = true;
	pthread_cond_broadcast(&proc->queue_cond);
	pthread_mutex_unlock(&proc->mtx);

	for (i = 0; i < num_workers; ++i) {
		if (proc->workers[i]->thread > 0) {
			pthread_join(proc->workers[i]->thread, NULL);
		}
	}
fail_init:
	for (i = 0; i < num_workers; ++i) {
		if (proc->workers[i] != NULL) {
			if (proc->workers[i]->cmp != NULL) {
				proc->workers[i]->cmp->
					destroy(proc->workers[i]->cmp);
			}

			free(proc->workers[i]);
		}
	}
	pthread_cond_destroy(&proc->done_cond);
	pthread_cond_destroy(&proc->queue_cond);
	pthread_mutex_destroy(&proc->mtx);
	free(proc);
	return NULL;
}

void sqfs_block_processor_destroy(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk;
	unsigned int i;

	pthread_mutex_lock(&proc->mtx);
	proc->terminate = true;
	pthread_cond_broadcast(&proc->queue_cond);
	pthread_mutex_unlock(&proc->mtx);

	for (i = 0; i < proc->num_workers; ++i) {
		pthread_join(proc->workers[i]->thread, NULL);

		proc->workers[i]->cmp->destroy(proc->workers[i]->cmp);
		free(proc->workers[i]);
	}

	pthread_cond_destroy(&proc->done_cond);
	pthread_cond_destroy(&proc->queue_cond);
	pthread_mutex_destroy(&proc->mtx);

	while (proc->queue != NULL) {
		blk = proc->queue;
		proc->queue = blk->next;
		free(blk);
	}

	while (proc->done != NULL) {
		blk = proc->done;
		proc->done = blk->next;
		free(blk);
	}

	free(proc);
}

static int process_completed_blocks(sqfs_block_processor_t *proc,
				    sqfs_block_t *queue)
{
	sqfs_block_t *it;
	int ret;

	while (queue != NULL) {
		it = queue;
		queue = queue->next;

		if (it->flags & SQFS_BLK_COMPRESS_ERROR) {
			proc->status = SQFS_ERROR_COMRPESSOR;
		} else {
			ret = proc->cb(proc->user, it);
			if (ret)
				proc->status = ret;
		}

		free(it);
	}

	return proc->status;
}

int sqfs_block_processor_enqueue(sqfs_block_processor_t *proc,
				 sqfs_block_t *block)
{
	sqfs_block_t *queue = NULL, *it, *prev;

	block->sequence_number = proc->enqueue_id++;
	block->next = NULL;

	pthread_mutex_lock(&proc->mtx);
	if ((block->flags & SQFS_BLK_DONT_COMPRESS) &&
	    (block->flags & SQFS_BLK_DONT_CHECKSUM)) {
		store_completed_block(proc, block);
	} else {
		while (proc->backlog > proc->max_backlog)
			pthread_cond_wait(&proc->done_cond, &proc->mtx);

		if (proc->queue_last == NULL) {
			proc->queue = proc->queue_last = block;
		} else {
			proc->queue_last->next = block;
			proc->queue_last = block;
		}

		proc->backlog += 1;
	}

	it = proc->done;
	prev = NULL;

	while (it != NULL && it->sequence_number == proc->dequeue_id) {
		prev = it;
		it = it->next;
		proc->dequeue_id += 1;
	}

	if (prev != NULL) {
		queue = proc->done;
		prev->next = NULL;
		proc->done = it;
	}

	pthread_cond_broadcast(&proc->queue_cond);
	pthread_mutex_unlock(&proc->mtx);

	return process_completed_blocks(proc, queue);
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	sqfs_block_t *queue, *it;

	pthread_mutex_lock(&proc->mtx);
	while (proc->backlog > 0)
		pthread_cond_wait(&proc->done_cond, &proc->mtx);

	for (it = proc->done; it != NULL; it = it->next) {
		if (it->sequence_number != proc->dequeue_id++) {
			pthread_mutex_unlock(&proc->mtx);

			/* XXX: this would actually be a BUG */
			return SQFS_ERROR_INTERNAL;
		}
	}

	queue = proc->done;
	proc->done = NULL;
	pthread_mutex_unlock(&proc->mtx);

	return process_completed_blocks(proc, queue);
}
