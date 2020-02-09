/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * common.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

void free_blk_list(sqfs_block_t *list)
{
	sqfs_block_t *it;

	while (list != NULL) {
		it = list;
		list = list->next;
		free(it);
	}
}

int block_processor_init(sqfs_block_processor_t *proc, size_t max_block_size,
			 sqfs_compressor_t *cmp, unsigned int num_workers,
			 size_t max_backlog, sqfs_block_writer_t *wr,
			 sqfs_frag_table_t *tbl)
{
	proc->max_block_size = max_block_size;
	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->cmp = cmp;
	proc->frag_tbl = tbl;
	proc->wr = wr;
	return 0;
}

void block_processor_cleanup(sqfs_block_processor_t *proc)
{
	free_blk_list(proc->queue);
	free_blk_list(proc->done);
	free(proc->blk_current);
	free(proc->frag_block);
	free(proc);
}

int sqfs_block_processor_set_hooks(sqfs_block_processor_t *proc, void *user_ptr,
				   const sqfs_block_hooks_t *hooks)
{
	proc->hooks = hooks;
	proc->user_ptr = user_ptr;

	return sqfs_block_writer_set_hooks(proc->wr, user_ptr, hooks);
}
