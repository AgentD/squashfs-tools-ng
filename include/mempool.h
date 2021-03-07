/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * mempool.h
 *
 * Copyright (C) 2021 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef MEMPOOL_H
#define MEMPOOL_H

#include "compat.h"

typedef struct mem_pool_t mem_pool_t;

#ifdef __cplusplus
extern "C" {
#endif

mem_pool_t *mem_pool_create(size_t obj_size);

void mem_pool_destroy(mem_pool_t *mem);

void *mem_pool_allocate(mem_pool_t *mem);

void mem_pool_free(mem_pool_t *mem, void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* MEMPOOL_H */
