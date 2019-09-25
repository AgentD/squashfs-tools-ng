/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * internal.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef INTERNAL_H
#define INTERNAL_H

#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/compress.h"
#include "sqfs/inode.h"
#include "sqfs/table.h"
#include "sqfs/error.h"
#include "sqfs/data.h"
#include "sqfs/io.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#ifdef WITH_PTHREAD
#include <pthread.h>
#endif


#define MK_BLK_HASH(chksum, size) \
	(((uint64_t)(size) << 32) | (uint64_t)(chksum))

#define INIT_BLOCK_COUNT (128)


typedef struct {
	uint64_t offset;
	uint64_t hash;
} blk_info_t;

typedef struct {
	uint32_t index;
	uint32_t offset;
	uint64_t hash;
} frag_info_t;


#ifdef WITH_PTHREAD
typedef struct {
	sqfs_block_processor_t *shared;
	sqfs_compressor_t *cmp;
	pthread_t thread;
	uint8_t scratch[];
} compress_worker_t;
#endif

struct sqfs_block_processor_t {
	/* synchronization primitives */
#ifdef WITH_PTHREAD
	pthread_mutex_t mtx;
	pthread_cond_t queue_cond;
	pthread_cond_t done_cond;
#endif

	/* needs rw access by worker and main thread */
	sqfs_block_t *queue;
	sqfs_block_t *queue_last;

	sqfs_block_t *done;
	size_t backlog;
	int status;

	/* used by main thread only */
	uint32_t enqueue_id;
	uint32_t dequeue_id;

	unsigned int num_workers;
	size_t max_backlog;

	size_t devblksz;
	sqfs_file_t *file;

	sqfs_fragment_t *fragments;
	size_t num_fragments;
	size_t max_fragments;

	uint64_t start;

	size_t file_start;
	size_t num_blocks;
	size_t max_blocks;
	blk_info_t *blocks;
	sqfs_compressor_t *cmp;

	sqfs_block_t *frag_block;
	frag_info_t *frag_list;
	size_t frag_list_num;
	size_t frag_list_max;

	/* used only by workers */
	size_t max_block_size;

#ifdef WITH_PTHREAD
	compress_worker_t *workers[];
#else
	uint8_t scratch[];
#endif
};

SQFS_INTERNAL int process_completed_block(sqfs_block_processor_t *proc,
					  sqfs_block_t *block);

SQFS_INTERNAL
int process_completed_fragment(sqfs_block_processor_t *proc, sqfs_block_t *frag,
			       sqfs_block_t **blk_out);

SQFS_INTERNAL void free_blk_list(sqfs_block_t *list);

SQFS_INTERNAL
int block_processor_init(sqfs_block_processor_t *proc, size_t max_block_size,
			 sqfs_compressor_t *cmp, unsigned int num_workers,
			 size_t max_backlog, size_t devblksz,
			 sqfs_file_t *file);

SQFS_INTERNAL void block_processor_cleanup(sqfs_block_processor_t *proc);

SQFS_INTERNAL
void block_processor_store_done(sqfs_block_processor_t *proc,
				sqfs_block_t *blk, int status);

SQFS_INTERNAL
sqfs_block_t *block_processor_next_work_item(sqfs_block_processor_t *proc);

SQFS_INTERNAL
int block_processor_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			     uint8_t *scratch, size_t scratch_size);

#endif /* INTERNAL_H */
