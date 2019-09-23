/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

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
	int status = 0;

	for (;;) {
		pthread_mutex_lock(&shared->mtx);
		if (blk != NULL) {
			store_completed_block(shared, blk);
			shared->backlog -= 1;

			if (status != 0 && shared->status == 0)
				shared->status = status;
			pthread_cond_broadcast(&shared->done_cond);
		}

		while (shared->queue == NULL && !shared->terminate &&
		       shared->status == 0) {
			pthread_cond_wait(&shared->queue_cond,
					  &shared->mtx);
		}

		if (shared->terminate || shared->status != 0) {
			pthread_mutex_unlock(&shared->mtx);
			break;
		}

		blk = shared->queue;
		shared->queue = blk->next;
		blk->next = NULL;

		if (shared->queue == NULL)
			shared->queue_last = NULL;
		pthread_mutex_unlock(&shared->mtx);

		status = sqfs_block_process(blk, worker->cmp, worker->scratch,
					    shared->max_block_size);
	}
	return NULL;
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    size_t devblksz,
						    sqfs_file_t *file)
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
	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->mtx = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	proc->queue_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	proc->done_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	proc->devblksz = devblksz;
	proc->cmp = cmp;
	proc->file = file;
	proc->max_blocks = INIT_BLOCK_COUNT;

	proc->blocks = alloc_array(sizeof(proc->blocks[0]), proc->max_blocks);
	if (proc->blocks == NULL)
		goto fail_init;

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
	free(proc->fragments);
	free(proc->blocks);
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

	free(proc->fragments);
	free(proc->blocks);
	free(proc);
}

int sqfs_block_processor_enqueue(sqfs_block_processor_t *proc,
				 sqfs_block_t *block)
{
	int status;

	pthread_mutex_lock(&proc->mtx);
	if (proc->status != 0) {
		status = proc->status;
		goto fail;
	}

	if (block->flags & ~SQFS_BLK_USER_SETTABLE_FLAGS) {
		status = SQFS_ERROR_UNSUPPORTED;
		proc->status = status;
		pthread_cond_broadcast(&proc->queue_cond);
		goto fail;
	}

	block->sequence_number = proc->enqueue_id++;
	block->next = NULL;

	if (block->size == 0) {
		block->checksum = 0;
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

	if (proc->done != NULL &&
	    proc->done->sequence_number == proc->dequeue_id) {
		block = proc->done;
		proc->done = proc->done->next;
		proc->dequeue_id += 1;
	} else {
		block = NULL;
	}

	pthread_cond_broadcast(&proc->queue_cond);
	pthread_mutex_unlock(&proc->mtx);

	if (block == NULL)
		return 0;

	status = process_completed_block(proc, block);
	if (status != 0) {
		pthread_mutex_lock(&proc->mtx);
		proc->status = status;
		goto fail;
	}

	free(block);
	return 0;
fail:
	pthread_mutex_unlock(&proc->mtx);
	free(block);
	return status;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	sqfs_block_t *queue, *it;
	int status;

	pthread_mutex_lock(&proc->mtx);
	while (proc->backlog > 0 && proc->status == 0)
		pthread_cond_wait(&proc->done_cond, &proc->mtx);

	if (proc->status != 0) {
		status = proc->status;
		goto fail;
	}

	for (it = proc->done; it != NULL; it = it->next) {
		if (it->sequence_number != proc->dequeue_id++) {
			status = SQFS_ERROR_INTERNAL;
			proc->status = status;
			goto fail;
		}
	}

	queue = proc->done;
	proc->done = NULL;
	pthread_mutex_unlock(&proc->mtx);

	while (queue != NULL) {
		it = queue;
		queue = queue->next;
		it->next = NULL;

		status = process_completed_block(proc, it);
		free(it);
		if (status != 0) {
			pthread_mutex_lock(&proc->mtx);
			proc->status = status;
			pthread_cond_broadcast(&proc->queue_cond);
			goto fail;
		}
	}

	return 0;
fail:
	while (proc->queue != NULL) {
		it = proc->queue;
		proc->queue = it->next;
		free(it);
	}

	while (proc->done != NULL) {
		it = proc->done;
		proc->done = it->next;
		free(it);
	}

	pthread_mutex_unlock(&proc->mtx);
	return status;
}
