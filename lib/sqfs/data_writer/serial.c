/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * serial.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

sqfs_data_writer_t *sqfs_data_writer_create(size_t max_block_size,
					    sqfs_compressor_t *cmp,
					    unsigned int num_workers,
					    size_t max_backlog,
					    size_t devblksz,
					    sqfs_file_t *file)
{
	sqfs_data_writer_t *proc;

	proc = alloc_flex(sizeof(*proc), 1, max_block_size);

	if (proc == NULL)
		return NULL;

	if (data_writer_init(proc, max_block_size, cmp, num_workers,
			     max_backlog, devblksz, file)) {
		data_writer_cleanup(proc);
		return NULL;
	}

	return proc;
}

void sqfs_data_writer_destroy(sqfs_data_writer_t *proc)
{
	data_writer_cleanup(proc);
}

int sqfs_data_writer_enqueue(sqfs_data_writer_t *proc, sqfs_block_t *block)
{
	sqfs_block_t *fragblk = NULL;

	if (proc->status != 0) {
		free(block);
		return proc->status;
	}

	if (block->flags & ~SQFS_BLK_USER_SETTABLE_FLAGS) {
		proc->status = SQFS_ERROR_UNSUPPORTED;
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

	proc->status = data_writer_do_block(block, proc->cmp, proc->scratch,
					    proc->max_block_size);

	if (proc->status == 0)
		proc->status = process_completed_block(proc, block);

	free(block);
	return proc->status;
}

int sqfs_data_writer_finish(sqfs_data_writer_t *proc)
{
	if (proc->status != 0 || proc->frag_block == NULL)
		return proc->status;

	proc->status = data_writer_do_block(proc->frag_block, proc->cmp,
					    proc->scratch,
					    proc->max_block_size);

	if (proc->status == 0)
		proc->status = process_completed_block(proc, proc->frag_block);

	free(proc->frag_block);
	proc->frag_block = NULL;
	return proc->status;
}
