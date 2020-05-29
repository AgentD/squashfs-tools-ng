/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * serial.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

typedef struct {
	sqfs_block_processor_t base;
	int status;
	sqfs_u8 scratch[];
} serial_block_processor_t;

static void free_block_list(sqfs_block_t *list)
{
	sqfs_block_t *blk;

	while (list != NULL) {
		blk = list;
		list = blk->next;
		free(blk);
	}
}

static void block_processor_destroy(sqfs_object_t *obj)
{
	sqfs_block_processor_t *proc = (sqfs_block_processor_t *)obj;

	free_block_list(proc->free_list);

	if (proc->frag_block != NULL) {
		free_block_list(proc->frag_block->frag_list);
		free(proc->frag_block);
	}

	free(proc->blk_current);
	free(proc);
}

static int append_to_work_queue(sqfs_block_processor_t *proc,
				sqfs_block_t *block)
{
	serial_block_processor_t *sproc = (serial_block_processor_t *)proc;
	sqfs_block_t *fragblk = NULL;

	if (sproc->status != 0)
		goto fail;

	sproc->status = proc->process_block(block, proc->cmp, sproc->scratch,
					    proc->max_block_size);
	if (sproc->status != 0)
		goto fail;

	if (block->flags & SQFS_BLK_IS_FRAGMENT) {
		sproc->status = proc->process_completed_fragment(proc, block,
								 &fragblk);
		if (fragblk == NULL)
			return sproc->status;

		block = fragblk;
		sproc->status = proc->process_block(block, proc->cmp,
						    sproc->scratch,
						    proc->max_block_size);
		if (sproc->status != 0)
			goto fail;
	}

	sproc->status = proc->process_completed_block(proc, block);
	return sproc->status;
fail:
	free_block_list(block->frag_list);
	free(block);
	return sproc->status;
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	serial_block_processor_t *proc;
	(void)num_workers; (void)max_backlog;

	proc = alloc_flex(sizeof(*proc), 1, max_block_size);
	if (proc == NULL)
		return NULL;

	if (block_processor_init(&proc->base, max_block_size, cmp, wr, tbl)) {
		free(proc);
		return NULL;
	}

	proc->base.append_to_work_queue = append_to_work_queue;
	((sqfs_object_t *)proc)->destroy = block_processor_destroy;
	return (sqfs_block_processor_t *)proc;
}

int sqfs_block_processor_sync(sqfs_block_processor_t *proc)
{
	return ((serial_block_processor_t *)proc)->status;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	serial_block_processor_t *sproc = (serial_block_processor_t *)proc;

	if (proc->frag_block == NULL || sproc->status != 0)
		goto fail;

	sproc->status = proc->process_block(proc->frag_block, proc->cmp,
					    sproc->scratch,
					    proc->max_block_size);
	if (sproc->status != 0)
		goto fail;

	sproc->status = proc->process_completed_block(proc, proc->frag_block);
	proc->frag_block = NULL;
	return sproc->status;
fail:
	if (proc->frag_block != NULL) {
		free_block_list(proc->frag_block->frag_list);
		free(proc->frag_block);
		proc->frag_block = NULL;
	}
	return sproc->status;
}
