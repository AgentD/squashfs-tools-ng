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
#include "util.h"

#include <string.h>
#include <stdlib.h>

typedef struct sqfs_block_t {
	struct sqfs_block_t *next;
	sqfs_inode_generic_t *inode;

	sqfs_u32 proc_seq_num;
	sqfs_u32 io_seq_num;
	sqfs_u32 flags;
	sqfs_u32 size;
	sqfs_u32 checksum;

	/* Data block index within the inode or fragment block index. */
	sqfs_u32 index;

	sqfs_u8 data[];
} sqfs_block_t;

struct sqfs_block_processor_t {
	sqfs_object_t obj;

	sqfs_frag_table_t *frag_tbl;
	sqfs_compressor_t *cmp;
	sqfs_block_t *frag_block;
	sqfs_block_writer_t *wr;

	sqfs_block_processor_stats_t stats;

	sqfs_inode_generic_t *inode;
	sqfs_block_t *blk_current;
	sqfs_u32 blk_flags;
	sqfs_u32 blk_index;

	size_t max_block_size;
};

SQFS_INTERNAL int process_completed_block(sqfs_block_processor_t *proc,
					  sqfs_block_t *block);

SQFS_INTERNAL
int process_completed_fragment(sqfs_block_processor_t *proc, sqfs_block_t *frag,
			       sqfs_block_t **blk_out);

SQFS_INTERNAL
int block_processor_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			     sqfs_u8 *scratch, size_t scratch_size);

SQFS_INTERNAL
int append_to_work_queue(sqfs_block_processor_t *proc, sqfs_block_t *block);

SQFS_INTERNAL sqfs_u32 xxh32(const void *input, const size_t len);

#endif /* INTERNAL_H */
