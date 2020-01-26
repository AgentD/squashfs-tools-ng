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

int data_writer_init(sqfs_data_writer_t *proc, size_t max_block_size,
		     sqfs_compressor_t *cmp, unsigned int num_workers,
		     size_t max_backlog, size_t devblksz, sqfs_file_t *file)
{
	proc->max_block_size = max_block_size;
	proc->num_workers = num_workers;
	proc->max_backlog = max_backlog;
	proc->devblksz = devblksz;
	proc->cmp = cmp;
	proc->file = file;
	proc->max_blocks = INIT_BLOCK_COUNT;

	proc->frag_tbl = sqfs_frag_table_create(0);
	if (proc->frag_tbl == NULL)
		return -1;

	proc->blocks = alloc_array(sizeof(proc->blocks[0]), proc->max_blocks);
	if (proc->blocks == NULL)
		return -1;

	return 0;
}

void data_writer_cleanup(sqfs_data_writer_t *proc)
{
	if (proc->frag_tbl != NULL)
		sqfs_frag_table_destroy(proc->frag_tbl);
	free_blk_list(proc->queue);
	free_blk_list(proc->done);
	free(proc->blk_current);
	free(proc->frag_block);
	free(proc->blocks);
	free(proc);
}

int sqfs_data_writer_write_fragment_table(sqfs_data_writer_t *proc,
					  sqfs_super_t *super)
{
	return sqfs_frag_table_write(proc->frag_tbl, proc->file,
				     super, proc->cmp);
}

int sqfs_data_writer_set_hooks(sqfs_data_writer_t *proc, void *user_ptr,
			       const sqfs_block_hooks_t *hooks)
{
	if (hooks->size != sizeof(*hooks))
		return SQFS_ERROR_UNSUPPORTED;

	proc->hooks = hooks;
	proc->user_ptr = user_ptr;
	return 0;
}
