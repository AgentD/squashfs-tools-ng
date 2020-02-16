/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * serial.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static void block_processor_destroy(sqfs_object_t *obj)
{
	sqfs_block_processor_t *proc = (sqfs_block_processor_t *)obj;

	free(proc->blk_current);
	free(proc->frag_block);
	free(proc);
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	sqfs_block_processor_t *proc;
	(void)num_workers; (void)max_backlog;

	proc = alloc_flex(sizeof(*proc), 1, max_block_size);
	if (proc == NULL)
		return NULL;

	proc->max_block_size = max_block_size;
	proc->cmp = cmp;
	proc->frag_tbl = tbl;
	proc->wr = wr;
	proc->stats.size = sizeof(proc->stats);
	((sqfs_object_t *)proc)->destroy = block_processor_destroy;
	return proc;
}

int append_to_work_queue(sqfs_block_processor_t *proc, sqfs_block_t *block)
{
	sqfs_block_t *fragblk = NULL;

	if (proc->status != 0)
		goto done;

	proc->status = block_processor_do_block(block, proc->cmp,
						proc->scratch,
						proc->max_block_size);
	if (proc->status != 0)
		goto done;

	if (block->flags & SQFS_BLK_IS_FRAGMENT) {
		proc->status = process_completed_fragment(proc, block,
							  &fragblk);
		if (fragblk == NULL)
			goto done;

		free(block);
		block = fragblk;

		proc->status = block_processor_do_block(block, proc->cmp,
							proc->scratch,
							proc->max_block_size);
		if (proc->status != 0)
			goto done;
	}

	proc->status = process_completed_block(proc, block);
done:
	free(block);
	return proc->status;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	if (proc->frag_block == NULL || proc->status != 0)
		goto out;

	proc->status = block_processor_do_block(proc->frag_block, proc->cmp,
						proc->scratch,
						proc->max_block_size);
	if (proc->status != 0)
		goto out;

	proc->status = process_completed_block(proc, proc->frag_block);
out:
	free(proc->frag_block);
	proc->frag_block = NULL;
	return proc->status;
}
