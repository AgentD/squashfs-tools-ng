/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * serial.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	sqfs_block_processor_t *proc;

	proc = alloc_flex(sizeof(*proc), 1, max_block_size);

	if (proc == NULL)
		return NULL;

	if (block_processor_init(proc, max_block_size, cmp, num_workers,
				 max_backlog, wr, tbl)) {
		block_processor_cleanup(proc);
		return NULL;
	}

	return proc;
}

void sqfs_block_processor_destroy(sqfs_block_processor_t *proc)
{
	block_processor_cleanup(proc);
}

int test_and_set_status(sqfs_block_processor_t *proc, int status)
{
	if (proc->status == 0)
		proc->status = status;

	return proc->status;
}

int append_to_work_queue(sqfs_block_processor_t *proc, sqfs_block_t *block,
			 bool signal_threads)
{
	sqfs_block_t *fragblk = NULL;
	(void)signal_threads;

	if (proc->status != 0 || block == NULL) {
		free(block);
		return proc->status;
	}

	if (block->flags & SQFS_BLK_IS_FRAGMENT) {
		block->checksum = crc32(0, block->data, block->size);

		proc->status = process_completed_fragment(proc, block,
							  &fragblk);
		free(block);

		if (proc->status != 0) {
			free(fragblk);
			return proc->status;
		}

		if (fragblk == NULL)
			return 0;

		block = fragblk;
	}

	proc->status = block_processor_do_block(block, proc->cmp, proc->scratch,
						proc->max_block_size);

	if (proc->status == 0)
		proc->status = process_completed_block(proc, block);

	free(block);
	return proc->status;
}

int wait_completed(sqfs_block_processor_t *proc)
{
	return proc->status;
}
