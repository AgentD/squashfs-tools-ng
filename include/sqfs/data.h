/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef SQFS_DATA_H
#define SQFS_DATA_H

#include "config.h"

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define SQFS_META_BLOCK_SIZE 8192

#define SQFS_IS_BLOCK_COMPRESSED(size) (((size) & (1 << 24)) == 0)
#define SQFS_ON_DISK_BLOCK_SIZE(size) ((size) & ((1 << 24) - 1))
#define SQFS_IS_SPARSE_BLOCK(size) (SQFS_ON_DISK_BLOCK_SIZE(size) == 0)

typedef struct {
	uint64_t start_offset;
	uint32_t size;
	uint32_t pad0;
} sqfs_fragment_t;

#endif /* SQFS_DATA_H */
