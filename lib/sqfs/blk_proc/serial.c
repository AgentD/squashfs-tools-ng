/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    size_t devblksz,
						    sqfs_file_t *file)
{
	sqfs_block_processor_t *proc;
	(void)num_workers;
	(void)max_backlog;

	proc = alloc_flex(sizeof(*proc), 1, max_block_size);

	if (proc == NULL)
		return NULL;

	proc->max_block_size = max_block_size;
	proc->cmp = cmp;
	proc->devblksz = devblksz;
	proc->file = file;
	proc->max_blocks = INIT_BLOCK_COUNT;
	proc->frag_list_max = INIT_BLOCK_COUNT;

	proc->blocks = alloc_array(sizeof(proc->blocks[0]), proc->max_blocks);
	if (proc->blocks == NULL) {
		free(proc);
		return NULL;
	}

	proc->frag_list = alloc_array(sizeof(proc->frag_list[0]),
				      proc->frag_list_max);
	if (proc->frag_list == NULL) {
		free(proc->blocks);
		free(proc);
		return NULL;
	}

	return proc;
}

void sqfs_block_processor_destroy(sqfs_block_processor_t *proc)
{
	free(proc->frag_block);
	free(proc->frag_list);
	free(proc->fragments);
	free(proc->blocks);
	free(proc);
}

int sqfs_block_processor_enqueue(sqfs_block_processor_t *proc,
				 sqfs_block_t *block)
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

		proc->status = handle_fragment(proc, block, &fragblk);
		free(block);

		if (proc->status != 0) {
			free(fragblk);
			return proc->status;
		}

		if (fragblk == NULL)
			return 0;

		block = fragblk;
	}

	proc->status = sqfs_block_process(block, proc->cmp, proc->scratch,
					  proc->max_block_size);

	if (proc->status == 0)
		proc->status = process_completed_block(proc, block);

	free(block);
	return proc->status;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	if (proc->status != 0 || proc->frag_block == NULL)
		return proc->status;

	proc->status = sqfs_block_process(proc->frag_block, proc->cmp,
					  proc->scratch, proc->max_block_size);

	if (proc->status == 0)
		proc->status = process_completed_block(proc, proc->frag_block);

	free(proc->frag_block);
	proc->frag_block = NULL;
	return proc->status;
}
