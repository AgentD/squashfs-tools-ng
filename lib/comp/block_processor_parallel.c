/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "block_processor.h"
#include "util.h"

#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_BACKLOG_FACTOR (10)

typedef struct {
	block_processor_t *shared;
	compressor_t *cmp;
	pthread_t thread;
	uint8_t scratch[];
} compress_worker_t;

struct block_processor_t {
	pthread_mutex_t mtx;
	pthread_cond_t queue_cond;
	pthread_cond_t done_cond;

	/* needs rw access by worker and main thread */
	block_t *queue;
	block_t *done;
	bool terminate;
	size_t backlog;

	/* used by main thread only */
	uint32_t enqueue_id;
	uint32_t dequeue_id;

	unsigned int num_workers;
	block_cb cb;
	void *user;
	int status;

	/* used only by workers */
	size_t max_block_size;

	compress_worker_t *workers[];
};

static void store_completed_block(block_processor_t *shared, block_t *blk)
{
	block_t *it = shared->done, *prev = NULL;

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
	block_processor_t *shared = worker->shared;
	block_t *blk;

	for (;;) {
		pthread_mutex_lock(&shared->mtx);
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
		pthread_mutex_unlock(&shared->mtx);

		if (process_block(blk, worker->cmp, worker->scratch,
				  shared->max_block_size)) {
			blk->flags |= BLK_COMPRESS_ERROR;
		}

		pthread_mutex_lock(&shared->mtx);
		store_completed_block(shared, blk);
		shared->backlog -= 1;
		pthread_cond_broadcast(&shared->done_cond);
		pthread_mutex_unlock(&shared->mtx);
	}
	return NULL;
}

block_processor_t *block_processor_create(size_t max_block_size,
					  compressor_t *cmp,
					  unsigned int num_workers,
					  void *user,
					  block_cb callback)
{
	block_processor_t *proc;
	unsigned int i;
	size_t size;
	int ret;

	if (num_workers < 1)
		num_workers = 1;

	size = sizeof(proc->workers[0]) * num_workers;

	proc = calloc(1, sizeof(*proc) + size);
	if (proc == NULL) {
		perror("Creating block processor");
		return NULL;
	}

	proc->max_block_size = max_block_size;
	proc->cb = callback;
	proc->user = user;
	proc->num_workers = num_workers;

	if (pthread_mutex_init(&proc->mtx, NULL)) {
		perror("Creating block processor mutex");
		goto fail_free;
	}

	if (pthread_cond_init(&proc->queue_cond, NULL)) {
		perror("Creating block processor conditional");
		goto fail_mtx;
	}

	if (pthread_cond_init(&proc->done_cond, NULL)) {
		perror("Creating block processor completion conditional");
		goto fail_cond;
	}

	size = sizeof(compress_worker_t) + max_block_size;

	for (i = 0; i < num_workers; ++i) {
		proc->workers[i] = calloc(1, size);

		if (proc->workers[i] == NULL) {
			perror("Creating block worker data");
			goto fail_init;
		}

		proc->workers[i]->shared = proc;
		proc->workers[i]->cmp = cmp->create_copy(cmp);

		if (proc->workers[i]->cmp == NULL)
			goto fail_init;
	}

	for (i = 0; i < num_workers; ++i) {
		ret = pthread_create(&proc->workers[i]->thread, NULL,
				     worker_proc, proc->workers[i]);

		if (ret != 0) {
			perror("Creating block processor thread");
			goto fail_thread;
		}
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
fail_cond:
	pthread_cond_destroy(&proc->queue_cond);
fail_mtx:
	pthread_mutex_destroy(&proc->mtx);
fail_free:
	free(proc);
	return NULL;
}

void block_processor_destroy(block_processor_t *proc)
{
	unsigned int i;
	block_t *blk;

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

static int process_completed_blocks(block_processor_t *proc, block_t *queue)
{
	block_t *it;

	while (queue != NULL) {
		it = queue;
		queue = queue->next;

		if (it->flags & BLK_COMPRESS_ERROR) {
			proc->status = -1;
		} else {
			if (proc->cb(proc->user, it))
				proc->status = -1;
		}

		free(it);
	}

	return proc->status;
}

int block_processor_enqueue(block_processor_t *proc, block_t *block)
{
	block_t *queue = NULL, *it, *prev;

	block->sequence_number = proc->enqueue_id++;
	block->next = NULL;

	pthread_mutex_lock(&proc->mtx);
	while (proc->backlog > proc->num_workers * MAX_BACKLOG_FACTOR)
		pthread_cond_wait(&proc->done_cond, &proc->mtx);

	if (proc->queue == NULL) {
		proc->queue = block;
	} else {
		for (it = proc->queue; it->next != NULL; it = it->next)
			;
		it->next = block;
	}

	proc->backlog += 1;
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

int block_processor_finish(block_processor_t *proc)
{
	block_t *queue, *it;

	pthread_mutex_lock(&proc->mtx);
	while (proc->backlog > 0)
		pthread_cond_wait(&proc->done_cond, &proc->mtx);

	for (it = proc->done; it != NULL; it = it->next) {
		if (it->sequence_number != proc->dequeue_id++) {
			pthread_mutex_unlock(&proc->mtx);
			goto bug_seqnum;
		}
	}

	queue = proc->done;
	proc->done = NULL;
	pthread_mutex_unlock(&proc->mtx);

	return process_completed_blocks(proc, queue);
bug_seqnum:
	fputs("[BUG][parallel block processor] "
	      "gap in sequence numbers!\n", stderr);
	return -1;
}
