/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static void free_blk_list(sqfs_block_t *list)
{
	sqfs_block_t *it;

	while (list != NULL) {
		it = list;
		list = list->next;
		free(it);
	}
}

static void store_completed_block(sqfs_block_processor_t *proc,
				  sqfs_block_t *blk, int status)
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
	pthread_cond_broadcast(&proc->done_cond);
}

static sqfs_block_t *get_next_work_item(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk;

	while (proc->queue == NULL) {
		if (proc->terminate || proc->status != 0)
			return NULL;

		pthread_cond_wait(&proc->queue_cond, &proc->mtx);
	}

	blk = proc->queue;
	proc->queue = blk->next;
	blk->next = NULL;

	if (proc->queue == NULL)
		proc->queue_last = NULL;

	return blk;
}

static void *worker_proc(void *arg)
{
	compress_worker_t *worker = arg;
	sqfs_block_processor_t *shared = worker->shared;
	sqfs_block_t *blk = NULL;
	int status = 0;

	for (;;) {
		pthread_mutex_lock(&shared->mtx);
		if (blk != NULL)
			store_completed_block(shared, blk, status);

		blk = get_next_work_item(shared);
		pthread_mutex_unlock(&shared->mtx);

		if (blk == NULL)
			break;

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
	proc->frag_list_max = INIT_BLOCK_COUNT;

	proc->blocks = alloc_array(sizeof(proc->blocks[0]), proc->max_blocks);
	if (proc->blocks == NULL)
		goto fail_init;

	proc->frag_list = alloc_array(sizeof(proc->frag_list[0]),
				      proc->frag_list_max);
	if (proc->frag_list == NULL)
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
	free(proc->frag_list);
	free(proc->fragments);
	free(proc->blocks);
	free(proc);
	return NULL;
}

void sqfs_block_processor_destroy(sqfs_block_processor_t *proc)
{
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

	free_blk_list(proc->queue);
	free_blk_list(proc->done);
	free(proc->frag_block);
	free(proc->frag_list);
	free(proc->fragments);
	free(proc->blocks);
	free(proc);
}

static void append_to_work_queue(sqfs_block_processor_t *proc,
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
	pthread_cond_broadcast(&proc->queue_cond);
}

static sqfs_block_t *get_completed_if_avail(sqfs_block_processor_t *proc)
{
	sqfs_block_t *block = NULL;

	if (proc->done != NULL &&
	    proc->done->sequence_number == proc->dequeue_id) {
		block = proc->done;
		proc->done = proc->done->next;
		proc->dequeue_id += 1;
	}

	return block;
}

static int test_and_set_status(sqfs_block_processor_t *proc, int status)
{
	pthread_mutex_lock(&proc->mtx);
	if (proc->status == 0) {
		proc->status = status;
	} else {
		status = proc->status;
	}
	pthread_cond_broadcast(&proc->queue_cond);
	return status;
}

static int queue_pump(sqfs_block_processor_t *proc, sqfs_block_t *block)
{
	sqfs_block_t *completed = NULL;
	int status;

	pthread_mutex_lock(&proc->mtx);
	while (proc->backlog > proc->max_backlog && proc->status == 0)
		pthread_cond_wait(&proc->done_cond, &proc->mtx);

	if (proc->status != 0) {
		status = proc->status;
		pthread_mutex_unlock(&proc->mtx);
		free(block);
		return status;
	}

	completed = get_completed_if_avail(proc);

	append_to_work_queue(proc, block);
	block = NULL;
	pthread_mutex_unlock(&proc->mtx);

	if (completed == NULL)
		return 0;

	if (completed->flags & SQFS_BLK_IS_FRAGMENT) {
		status = handle_fragment(proc, completed, &block);

		if (status != 0) {
			free(block);
			status = test_and_set_status(proc, status);
		} else if (block != NULL) {
			pthread_mutex_lock(&proc->mtx);
			proc->dequeue_id = completed->sequence_number;
			block->sequence_number = proc->dequeue_id;

			if (proc->queue == NULL) {
				proc->queue = block;
				proc->queue_last = block;
			} else {
				block->next = proc->queue;
				proc->queue = block;
			}

			proc->backlog += 1;
			pthread_cond_broadcast(&proc->queue_cond);
			pthread_mutex_unlock(&proc->mtx);
		}
	} else {
		status = process_completed_block(proc, completed);

		if (status != 0)
			status = test_and_set_status(proc, status);
	}

	free(completed);
	return status;
}

int sqfs_block_processor_enqueue(sqfs_block_processor_t *proc,
				 sqfs_block_t *block)
{
	if (block->flags & ~SQFS_BLK_USER_SETTABLE_FLAGS) {
		free(block);
		return test_and_set_status(proc, SQFS_ERROR_UNSUPPORTED);
	}

	return queue_pump(proc, block);
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	sqfs_block_t *it, *block;
	int status = 0;

	pthread_mutex_lock(&proc->mtx);
restart:
	while (proc->backlog > 0 && proc->status == 0)
		pthread_cond_wait(&proc->done_cond, &proc->mtx);

	if (proc->status != 0) {
		status = proc->status;
		pthread_mutex_unlock(&proc->mtx);
		return status;
	}

	while (proc->done != NULL) {
		it = get_completed_if_avail(proc);

		if (it == NULL) {
			status = SQFS_ERROR_INTERNAL;
		} else if (it->flags & SQFS_BLK_IS_FRAGMENT) {
			block = NULL;
			status = handle_fragment(proc, it, &block);

			if (status != 0) {
				proc->status = status;
				pthread_mutex_unlock(&proc->mtx);
				free(block);
				free(it);
				return status;
			}

			if (block != NULL) {
				proc->dequeue_id = it->sequence_number;
				block->sequence_number = proc->dequeue_id;
				free(it);

				if (proc->queue == NULL) {
					proc->queue = block;
					proc->queue_last = block;
				} else {
					block->next = proc->queue;
					proc->queue = block;
				}

				proc->backlog += 1;
				pthread_cond_broadcast(&proc->queue_cond);
				goto restart;
			}

			free(it);
		} else {
			status = process_completed_block(proc, it);
			free(it);

			if (status != 0) {
				proc->status = status;
				pthread_mutex_unlock(&proc->mtx);
				return status;
			}
		}
	}

	if (proc->frag_block != NULL) {
		append_to_work_queue(proc, proc->frag_block);
		proc->frag_block = NULL;
		goto restart;
	}

	pthread_mutex_unlock(&proc->mtx);
	return 0;
}
