/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * threadpool.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "threadpool.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(__WINDOWS__)
#include "w32threadwrap.h"

#define THREAD_FUN(funname, argname) DWORD WINAPI funname(LPVOID argname)
#define THREAD_EXIT_SUCCESS (0)
#else
#include <pthread.h>
#include <signal.h>

#define THREAD_FUN(funname, argname) void *funname(void *argname)
#define THREAD_EXIT_SUCCESS NULL
#endif

typedef struct thread_pool_impl_t thread_pool_impl_t;

typedef struct work_item_t {
	struct work_item_t *next;
	size_t ticket_number;

	void *data;
} work_item_t;

typedef struct {
	pthread_t thread;
	thread_pool_impl_t *pool;

	thread_pool_worker_t fun;
	void *user;
} worker_t;

struct thread_pool_impl_t {
	thread_pool_t base;

	pthread_mutex_t mtx;
	pthread_cond_t queue_cond;
	pthread_cond_t done_cond;

	size_t next_ticket;
	size_t next_dequeue_ticket;

	size_t item_count;

	work_item_t *queue;
	work_item_t *queue_last;

	work_item_t *done;

	work_item_t *safe_done;
	work_item_t *safe_done_last;

	work_item_t *recycle;

	int status;

	size_t num_workers;
	worker_t workers[];
};

/*****************************************************************************/

static void store_completed(thread_pool_impl_t *pool, work_item_t *done,
			    int status)
{
	work_item_t *it = pool->done, *prev = NULL;

	while (it != NULL) {
		if (it->ticket_number >= done->ticket_number)
			break;

		prev = it;
		it = it->next;
	}

	if (prev == NULL) {
		done->next = pool->done;
		pool->done = done;
	} else {
		done->next = prev->next;
		prev->next = done;
	}

	if (status != 0 && pool->status == 0)
		pool->status = status;

	pthread_cond_broadcast(&pool->done_cond);
}

static work_item_t *get_next_work_item(thread_pool_impl_t *pool)
{
	work_item_t *item = NULL;

	while (pool->queue == NULL && pool->status == 0)
		pthread_cond_wait(&pool->queue_cond, &pool->mtx);

	if (pool->status == 0) {
		item = pool->queue;
		pool->queue = item->next;
		item->next = NULL;

		if (pool->queue == NULL)
			pool->queue_last = NULL;
	}

	return item;
}

static THREAD_FUN(worker_proc, arg)
{
	work_item_t *item = NULL;
	worker_t *worker = arg;
	int status = 0;

	for (;;) {
		pthread_mutex_lock(&worker->pool->mtx);
		if (item != NULL)
			store_completed(worker->pool, item, status);

		item = get_next_work_item(worker->pool);
		pthread_mutex_unlock(&worker->pool->mtx);

		if (item == NULL)
			break;

		status = worker->fun(worker->user, item->data);
	}

	return THREAD_EXIT_SUCCESS;
}

/*****************************************************************************/

static work_item_t *try_dequeue_done(thread_pool_impl_t *pool)
{
	work_item_t *out;

	if (pool->done == NULL)
		return NULL;

	if (pool->done->ticket_number != pool->next_dequeue_ticket)
		return NULL;

	out = pool->done;
	pool->done = out->next;
	out->next = NULL;
	pool->next_dequeue_ticket += 1;
	return out;
}

static void free_item_list(work_item_t *list)
{
	while (list != NULL) {
		work_item_t *item = list;
		list = list->next;
		free(item);
	}
}

static void destroy(thread_pool_t *interface)
{
	thread_pool_impl_t *pool = (thread_pool_impl_t *)interface;
	size_t i;

	pthread_mutex_lock(&pool->mtx);
	pool->status = -1;
	pthread_cond_broadcast(&pool->queue_cond);
	pthread_mutex_unlock(&pool->mtx);

	for (i = 0; i < pool->num_workers; ++i)
		pthread_join(pool->workers[i].thread, NULL);

	pthread_cond_destroy(&pool->done_cond);
	pthread_cond_destroy(&pool->queue_cond);
	pthread_mutex_destroy(&pool->mtx);

	free_item_list(pool->queue);
	free_item_list(pool->done);
	free_item_list(pool->recycle);
	free_item_list(pool->safe_done);
	free(pool);
}

static size_t get_worker_count(thread_pool_t *interface)
{
	thread_pool_impl_t *pool = (thread_pool_impl_t *)interface;

	return pool->num_workers;
}

static void set_worker_ptr(thread_pool_t *interface, size_t idx, void *ptr)
{
	thread_pool_impl_t *pool = (thread_pool_impl_t *)interface;

	if (idx >= pool->num_workers)
		return;

	pthread_mutex_lock(&pool->mtx);
	pool->workers[idx].user = ptr;
	pthread_mutex_unlock(&pool->mtx);
}

static int submit(thread_pool_t *interface, void *ptr)
{
	thread_pool_impl_t *pool = (thread_pool_impl_t *)interface;
	work_item_t *item = NULL;
	int status;

	if (pool->recycle != NULL) {
		item = pool->recycle;
		pool->recycle = item->next;
		item->next = NULL;
	}

	if (item == NULL) {
		item = calloc(1, sizeof(*item));
		if (item == NULL)
			return -1;
	}

	pthread_mutex_lock(&pool->mtx);
	status = pool->status;

	if (status == 0) {
		item->data = ptr;
		item->ticket_number = pool->next_ticket++;

		if (pool->queue_last == NULL) {
			pool->queue = item;
		} else {
			pool->queue_last->next = item;
		}

		pool->queue_last = item;
		pool->item_count += 1;
	}

	for (;;) {
		work_item_t *done = try_dequeue_done(pool);
		if (done == NULL)
			break;

		if (pool->safe_done_last == NULL) {
			pool->safe_done = done;
		} else {
			pool->safe_done_last->next = done;
		}

		pool->safe_done_last = done;
	}

	pthread_cond_broadcast(&pool->queue_cond);
	pthread_mutex_unlock(&pool->mtx);

	if (status != 0) {
		memset(item, 0, sizeof(*item));
		item->next = pool->recycle;
		pool->recycle = item;
	}

	return status;
}

static void *dequeue(thread_pool_t *interface)
{
	thread_pool_impl_t *pool = (thread_pool_impl_t *)interface;
	work_item_t *out = NULL;
	void *ptr = NULL;

	if (pool->item_count == 0)
		return NULL;

	if (pool->safe_done != NULL) {
		out = pool->safe_done;

		pool->safe_done = pool->safe_done->next;
		if (pool->safe_done == NULL)
			pool->safe_done_last = NULL;
	} else {
		pthread_mutex_lock(&pool->mtx);
		for (;;) {
			out = try_dequeue_done(pool);
			if (out != NULL)
				break;

			pthread_cond_wait(&pool->done_cond, &pool->mtx);
		}
		pthread_mutex_unlock(&pool->mtx);
	}

	ptr = out->data;

	out->ticket_number = 0;
	out->data = NULL;
	out->next = pool->recycle;
	pool->recycle = out;

	pool->item_count -= 1;
	return ptr;
}

static int get_status(thread_pool_t *interface)
{
	thread_pool_impl_t *pool = (thread_pool_impl_t *)interface;
	int status;

	pthread_mutex_lock(&pool->mtx);
	status = pool->status;
	pthread_mutex_unlock(&pool->mtx);

	return status;
}

thread_pool_t *thread_pool_create(size_t num_jobs, thread_pool_worker_t worker)
{
	thread_pool_impl_t *pool;
	thread_pool_t *interface;
	sigset_t set, oldset;
	size_t i, j;
	int ret;

	if (num_jobs < 1)
		num_jobs = 1;

	pool = alloc_flex(sizeof(*pool), sizeof(pool->workers[0]), num_jobs);
	if (pool == NULL)
		return NULL;

	if (pthread_mutex_init(&pool->mtx, NULL) != 0)
		goto fail_free;

	if (pthread_cond_init(&pool->queue_cond, NULL) != 0)
		goto fail_mtx;

	if (pthread_cond_init(&pool->done_cond, NULL) != 0)
		goto fail_qcond;

	sigfillset(&set);
	pthread_sigmask(SIG_SETMASK, &set, &oldset);

	pool->num_workers = num_jobs;

	for (i = 0; i < num_jobs; ++i) {
		pool->workers[i].fun = worker;
		pool->workers[i].pool = pool;

		ret = pthread_create(&pool->workers[i].thread, NULL,
				     worker_proc, pool->workers + i);

		if (ret != 0)
			goto fail;
	}

	pthread_sigmask(SIG_SETMASK, &oldset, NULL);

	interface = (thread_pool_t *)pool;
	interface->destroy = destroy;
	interface->get_worker_count = get_worker_count;
	interface->set_worker_ptr = set_worker_ptr;
	interface->submit = submit;
	interface->dequeue = dequeue;
	interface->get_status = get_status;
	return interface;
fail:
	pthread_mutex_lock(&pool->mtx);
	pool->status = -1;
	pthread_cond_broadcast(&pool->queue_cond);
	pthread_mutex_unlock(&pool->mtx);

	for (j = 0; j < i; ++j)
		pthread_join(pool->workers[j].thread, NULL);

	pthread_sigmask(SIG_SETMASK, &oldset, NULL);
	pthread_cond_destroy(&pool->done_cond);
fail_qcond:
	pthread_cond_destroy(&pool->queue_cond);
fail_mtx:
	pthread_mutex_destroy(&pool->mtx);
fail_free:
	free(pool);
	return NULL;
}
