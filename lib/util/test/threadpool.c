/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * threadpool.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/threadpool.h"
#include "util/test.h"

#if defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

static CRITICAL_SECTION mutex;
static unsigned int ticket;

static void ticket_init(void)
{
	InitializeCriticalSection(&mutex);
	ticket = 0;
}

static void ticket_cleanup(void)
{
	DeleteCriticalSection(&mutex);
	ticket = 0;
}

static void ticket_wait(unsigned int value)
{
	for (;;) {
		EnterCriticalSection(&mutex);

		if (value == ticket) {
			ticket += 1;
			LeaveCriticalSection(&mutex);
			break;
		}

		LeaveCriticalSection(&mutex);
		SwitchToThread();
	}
}
#elif defined(HAVE_PTHREAD)
#include <pthread.h>

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static unsigned int ticket;

static void ticket_init(void)
{
	ticket = 0;
}

static void ticket_cleanup(void)
{
}

static void ticket_wait(unsigned int value)
{
	for (;;) {
		pthread_mutex_lock(&mutex);

		if (value == ticket) {
			ticket += 1;
			pthread_mutex_unlock(&mutex);
			break;
		}

		pthread_mutex_unlock(&mutex);
		sched_yield();
	}
}
#else
static void ticket_init(void)
{
}

static void ticket_cleanup(void)
{
}

static void ticket_wait(unsigned int value)
{
	(void)value;
}
#endif

static int worker(void *user, void *work_item)
{
	unsigned int value = *((unsigned int *)work_item);
	(void)user;

	ticket_wait(value);

	*((unsigned int *)work_item) = 42;
	return 0;
}

static int worker_serial(void *user, void *work_item)
{
	(void)user;
	*((unsigned int *)work_item) = 42;
	return 0;
}

static void test_case(thread_pool_t *pool)
{
	unsigned int values[10];
	unsigned int *ptr;
	size_t i, count;
	int ret;

	/* must return a sane value */
	count = pool->get_worker_count(pool);
	TEST_ASSERT(count >= 1);

	/* dequeue on empty pool MUST NOT lock up */
	ptr = pool->dequeue(pool);
	TEST_NULL(ptr);

	/* submit work items in reverse order */
	ticket_init();

	for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		values[i] = (sizeof(values) / sizeof(values[0]) - 1) - i;

		ret = pool->submit(pool, values + i);
		TEST_EQUAL_I(ret, 0);
	}

	/* items must dequeue in the same order */
	for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		ptr = pool->dequeue(pool);

		TEST_NOT_NULL(ptr);
		TEST_ASSERT(ptr == (values + i));
		TEST_EQUAL_UI(*ptr, 42);
	}

	ticket_cleanup();

	/* queue now empty */
	ptr = pool->dequeue(pool);
	TEST_NULL(ptr);
}

int main(int argc, char **argv)
{
	thread_pool_t *pool;
	(void)argc; (void)argv;

	/* test the actual parallel implementation */
	pool = thread_pool_create(10, worker);
	TEST_NOT_NULL(pool);
	test_case(pool);
	pool->destroy(pool);

	/* repeate the test with the serial reference implementation */
	pool = thread_pool_create_serial(worker_serial);
	TEST_NOT_NULL(pool);
	test_case(pool);
	pool->destroy(pool);
	return EXIT_SUCCESS;
}
