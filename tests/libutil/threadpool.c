/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * threadpool.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "threadpool.h"
#include "../test.h"

#if defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>
#endif

#include <time.h>

static int worker(void *user, void *work_item)
{
	unsigned int value = *((unsigned int *)work_item);
	(void)user;

#if defined(_WIN32) || defined(__WINDOWS__)
	Sleep(100 * value);
#else
	{
		struct timespec sp;

		sp.tv_sec = value / 10;
		sp.tv_nsec = 100000000;
		sp.tv_nsec *= (long)(value % 10);

		nanosleep(&sp, NULL);
	}
#endif

	*((unsigned int *)work_item) = 42;
	return 0;
}

int main(void)
{
	unsigned int values[10];
	thread_pool_t *pool;
	unsigned int *ptr;
	size_t i, count;
	int ret;

	pool = thread_pool_create(10, worker);
	TEST_NOT_NULL(pool);

	count = pool->get_worker_count(pool);
	TEST_ASSERT(count >= 1);

	/* dequeue on empty pool MUST NOT deadlock */
	ptr = pool->dequeue(pool);
	TEST_NULL(ptr);

	for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		values[i] = sizeof(values) / sizeof(values[0]) - i;

		ret = pool->submit(pool, values + i);
		TEST_EQUAL_I(ret, 0);
	}

	for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		ptr = pool->dequeue(pool);

		TEST_NOT_NULL(ptr);
		TEST_ASSERT(ptr == (values + i));
		TEST_EQUAL_UI(*ptr, 42);
	}

	ptr = pool->dequeue(pool);
	TEST_NULL(ptr);

	pool->destroy(pool);

	/* redo the same test with the serial implementation */
	pool = thread_pool_create_serial(worker);
	TEST_NOT_NULL(pool);

	ptr = pool->dequeue(pool);
	TEST_NULL(ptr);

	count = pool->get_worker_count(pool);
	TEST_EQUAL_UI(count, 1);

	for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		values[i] = sizeof(values) / sizeof(values[0]) - i;

		ret = pool->submit(pool, values + i);
		TEST_EQUAL_I(ret, 0);
	}

	for (i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
		ptr = pool->dequeue(pool);

		TEST_NOT_NULL(ptr);
		TEST_ASSERT(ptr == (values + i));
		TEST_EQUAL_UI(*ptr, 42);
	}

	ptr = pool->dequeue(pool);
	TEST_NULL(ptr);

	pool->destroy(pool);
	return EXIT_SUCCESS;
}
