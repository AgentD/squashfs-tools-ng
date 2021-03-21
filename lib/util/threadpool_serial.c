/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * threadpool_serial.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "threadpool.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

typedef struct work_item_t {
	struct work_item_t *next;
	void *data;
} work_item_t;

typedef struct {
	thread_pool_t base;

	work_item_t *queue;
	work_item_t *queue_last;

	work_item_t *recycle;

	thread_pool_worker_t fun;
	void *user;
	int status;
} serial_impl_t;

static void destroy(thread_pool_t *interface)
{
	serial_impl_t *pool = (serial_impl_t *)interface;

	while (pool->queue != NULL) {
		work_item_t *item = pool->queue;
		pool->queue = item->next;
		free(item);
	}

	while (pool->recycle != NULL) {
		work_item_t *item = pool->recycle;
		pool->recycle = item->next;
		free(item);
	}

	free(pool);
}

static size_t get_worker_count(thread_pool_t *pool)
{
	(void)pool;
	return 1;
}

static void set_worker_ptr(thread_pool_t *interface, size_t idx, void *ptr)
{
	serial_impl_t *pool = (serial_impl_t *)interface;

	if (idx >= 1)
		return;

	pool->user = ptr;
}

static int submit(thread_pool_t *interface, void *ptr)
{
	serial_impl_t *pool = (serial_impl_t *)interface;
	work_item_t *item = NULL;

	if (pool->status != 0)
		return pool->status;

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

	item->data = ptr;

	if (pool->queue_last == NULL) {
		pool->queue = item;
	} else {
		pool->queue_last->next = item;
	}

	pool->queue_last = item;
	return 0;
}

static void *dequeue(thread_pool_t *interface)
{
	serial_impl_t *pool = (serial_impl_t *)interface;
	work_item_t *item;
	void *ptr;
	int ret;

	if (pool->queue == NULL)
		return NULL;

	item = pool->queue;
	pool->queue = item->next;

	if (pool->queue == NULL)
		pool->queue_last = NULL;

	ptr = item->data;

	memset(item, 0, sizeof(*item));
	item->next = pool->recycle;
	pool->recycle = item;

	ret = pool->fun(pool->user, ptr);

	if (ret != 0 && pool->status == 0)
		pool->status = ret;

	return ptr;
}

static int get_status(thread_pool_t *interface)
{
	serial_impl_t *pool = (serial_impl_t *)interface;

	return pool->status;
}

thread_pool_t *thread_pool_create_serial(thread_pool_worker_t worker)
{
	serial_impl_t *pool = calloc(1, sizeof(*pool));
	thread_pool_t *interface = (thread_pool_t *)pool;

	if (pool == NULL)
		return NULL;

	pool->fun = worker;

	interface = (thread_pool_t *)pool;
	interface->destroy = destroy;
	interface->get_worker_count = get_worker_count;
	interface->set_worker_ptr = set_worker_ptr;
	interface->submit = submit;
	interface->dequeue = dequeue;
	interface->get_status = get_status;
	return interface;

}

#ifdef NO_THREAD_IMPL
thread_pool_t *thread_pool_create(size_t num_jobs, thread_pool_worker_t worker)
{
	(void)num_jobs;
	return thread_pool_create_serial(worker);
}
#endif
