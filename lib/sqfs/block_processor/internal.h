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
#include "sqfs/block_writer.h"
#include "sqfs/frag_table.h"
#include "sqfs/compressor.h"
#include "sqfs/inode.h"
#include "sqfs/table.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "../util.h"

#include <string.h>
#include <stdlib.h>
#include <zlib.h>

#ifdef WITH_PTHREAD
#include <pthread.h>
#include <signal.h>
#elif defined(_WIN32) || defined(__WINDOWS__)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

typedef struct compress_worker_t compress_worker_t;

typedef struct sqfs_block_t {
	struct sqfs_block_t *next;
	sqfs_inode_generic_t *inode;

	sqfs_u32 sequence_number;
	sqfs_u32 flags;
	sqfs_u32 size;
	sqfs_u32 checksum;

	/* Data block index within the inode or fragment block index. */
	sqfs_u32 index;

	sqfs_u8 data[];
} sqfs_block_t;

struct sqfs_block_processor_t {
	/* synchronization primitives */
#ifdef WITH_PTHREAD
	pthread_mutex_t mtx;
	pthread_cond_t queue_cond;
	pthread_cond_t done_cond;
#elif defined(_WIN32) || defined(__WINDOWS__)
	CRITICAL_SECTION mtx;
	CONDITION_VARIABLE queue_cond;
	CONDITION_VARIABLE done_cond;
#endif

	/* needs rw access by worker and main thread */
	sqfs_block_t *queue;
	sqfs_block_t *queue_last;

	sqfs_block_t *done;
	size_t backlog;
	int status;

	/* used by main thread only */
	sqfs_u32 enqueue_id;
	sqfs_u32 dequeue_id;

	unsigned int num_workers;
	size_t max_backlog;

	sqfs_frag_table_t *frag_tbl;
	sqfs_compressor_t *cmp;

	sqfs_block_t *frag_block;
	bool notify_threads;

	sqfs_block_writer_t *wr;
	sqfs_block_processor_stats_t stats;

	/* file API */
	sqfs_inode_generic_t *inode;
	sqfs_block_t *blk_current;
	sqfs_u32 blk_flags;
	size_t blk_index;

	/* used only by workers */
	size_t max_block_size;

#if defined(WITH_PTHREAD) || defined(_WIN32) || defined(__WINDOWS__)
	compress_worker_t *workers[];
#else
	sqfs_u8 scratch[];
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
			 size_t max_backlog, sqfs_block_writer_t *wr,
			 sqfs_frag_table_t *tbl);

SQFS_INTERNAL void block_processor_cleanup(sqfs_block_processor_t *proc);

SQFS_INTERNAL
int block_processor_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			     sqfs_u8 *scratch, size_t scratch_size);

SQFS_INTERNAL
int test_and_set_status(sqfs_block_processor_t *proc, int status);

SQFS_INTERNAL
int append_to_work_queue(sqfs_block_processor_t *proc, sqfs_block_t *block,
			 bool notify_threads);

SQFS_INTERNAL int wait_completed(sqfs_block_processor_t *proc);

#endif /* INTERNAL_H */
