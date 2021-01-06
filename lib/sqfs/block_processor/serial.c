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

static void block_processor_destroy(sqfs_object_t *obj)
{
	sqfs_block_processor_t *proc = (sqfs_block_processor_t *)obj;

	block_processor_cleanup(proc);
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
	free(block);
	return sproc->status;
}

static int block_processor_sync(sqfs_block_processor_t *proc)
{
	return ((serial_block_processor_t *)proc)->status;
}

int sqfs_block_processor_create_ex(const sqfs_block_processor_desc_t *desc,
				   sqfs_block_processor_t **out)
{
	serial_block_processor_t *proc;
	int ret;

	if (desc->size != sizeof(sqfs_block_processor_desc_t))
		return SQFS_ERROR_ARG_INVALID;

	proc = alloc_flex(sizeof(*proc), 1, desc->max_block_size);
	if (proc == NULL)
		return SQFS_ERROR_ALLOC;

	ret = block_processor_init(&proc->base, desc);
	if (ret != 0) {
		free(proc);
		return ret;
	}

	proc->base.sync = block_processor_sync;
	proc->base.append_to_work_queue = append_to_work_queue;
	((sqfs_object_t *)proc)->destroy = block_processor_destroy;

	*out = (sqfs_block_processor_t *)proc;
	return 0;
}
