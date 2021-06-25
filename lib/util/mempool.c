/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * mempool.c
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mempool.h"

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <assert.h>

#if defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#define DEF_POOL_SIZE (65536)
#define MEM_ALIGN (8)

typedef struct pool_t {
	struct pool_t *next;

	unsigned char *data;
	unsigned char *limit;

	unsigned int *bitmap;

	size_t obj_free;

	unsigned int blob[];
} pool_t;

struct mem_pool_t {
	size_t obj_size;
	size_t pool_size;
	size_t bitmap_count;
	pool_t *pool_list;
};

static size_t pool_size_from_bitmap_count(size_t count, size_t obj_size)
{
	size_t size, byte_count, bit_count;

	size = sizeof(pool_t);
	if (size % sizeof(unsigned int))
		size += sizeof(unsigned int) - size % sizeof(unsigned int);

	byte_count = count * sizeof(unsigned int);
	bit_count = byte_count * CHAR_BIT;

	size += byte_count;
	if (size % obj_size)
		size += obj_size - size % obj_size;

	size += bit_count * obj_size;
	return size;
}

static pool_t *create_pool(const mem_pool_t *mem)
{
	unsigned char *ptr;
	pool_t *pool;

#if defined(_WIN32) || defined(__WINDOWS__)
	pool = VirtualAlloc(NULL, mem->pool_size, MEM_RESERVE | MEM_COMMIT,
			    PAGE_READWRITE);

	if (pool == NULL)
		return NULL;
#else
	pool = mmap(NULL, mem->pool_size, PROT_READ | PROT_WRITE,
		    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (pool == MAP_FAILED)
		return NULL;
#endif
	pool->bitmap = pool->blob;
	pool->obj_free = mem->bitmap_count * sizeof(unsigned int) * CHAR_BIT;

	ptr = (unsigned char *)(pool->bitmap + mem->bitmap_count);

	if (((uintptr_t)ptr) % mem->obj_size) {
		ptr += mem->obj_size;
		ptr -= ((uintptr_t)ptr) % mem->obj_size;
	}

	pool->data = ptr;
	pool->limit = pool->data + pool->obj_free * mem->obj_size - 1;

	memset(pool->bitmap, 0, mem->bitmap_count * sizeof(unsigned int));
	return pool;
}

mem_pool_t *mem_pool_create(size_t obj_size)
{
	mem_pool_t *mem = calloc(1, sizeof(*mem));
	size_t count = 1, total;

	if (mem == NULL)
		return NULL;

	if (obj_size % MEM_ALIGN)
		obj_size += MEM_ALIGN - obj_size % MEM_ALIGN;

	for (;;) {
		total = pool_size_from_bitmap_count(count, obj_size);
		if (total > DEF_POOL_SIZE)
			break;
		++count;
	}

	--count;

	mem->obj_size = obj_size;
	mem->pool_size = DEF_POOL_SIZE;
	mem->bitmap_count = count;
	return mem;
}

void mem_pool_destroy(mem_pool_t *mem)
{
	while (mem->pool_list != NULL) {
		pool_t *pool = mem->pool_list;
		mem->pool_list = pool->next;

#if defined(_WIN32) || defined(__WINDOWS__)
		VirtualFree(pool, mem->pool_size, MEM_RELEASE);
#else
		munmap(pool, mem->pool_size);
#endif
	}

	free(mem);
}

void *mem_pool_allocate(mem_pool_t *mem)
{
	size_t idx, i, j;
	void *ptr = NULL;
	pool_t *it;

	for (it = mem->pool_list; it != NULL; it = it->next) {
		if (it->obj_free > 0)
			break;
	}

	if (it == NULL) {
		it = create_pool(mem);
		if (it == NULL)
			return NULL;

		it->next = mem->pool_list;
		mem->pool_list = it;
	}

	for (i = 0; i < mem->bitmap_count; ++i) {
		if (it->bitmap[i] < UINT_MAX)
			break;
	}

	for (j = 0; j < (sizeof(it->bitmap[i]) * CHAR_BIT); ++j) {
		if (!(it->bitmap[i] & (1UL << j)))
			break;
	}

	idx = i * sizeof(unsigned int) * CHAR_BIT + j;
	ptr = it->data + idx * mem->obj_size;

	it->bitmap[i] |= (1UL << j);
	it->obj_free -= 1;

	memset(ptr, 0, mem->obj_size);
	return ptr;
}

void mem_pool_free(mem_pool_t *mem, void *ptr)
{
	size_t idx, i, j;
	pool_t *it;

	for (it = mem->pool_list; it != NULL; it = it->next) {
		if ((unsigned char *)ptr >= it->data &&
		    (unsigned char *)ptr < it->limit) {
			break;
		}
	}

	assert(it != NULL);

	idx = (size_t)((unsigned char *)ptr - it->data);

	assert((idx % mem->obj_size) == 0);
	idx /= mem->obj_size;

	i = idx / (sizeof(unsigned int) * CHAR_BIT);
	j = idx % (sizeof(unsigned int) * CHAR_BIT);

	assert((it->bitmap[i] & (1 << j)) != 0);

	it->bitmap[i] &= ~(1 << j);
}
