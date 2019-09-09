/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/error.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

struct sqfs_block_processor_t {
	size_t max_block_size;
	sqfs_compressor_t *cmp;
	sqfs_block_cb cb;
	void *user;
	int status;

	uint8_t scratch[];
};

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    void *user,
						    sqfs_block_cb callback)
{
	sqfs_block_processor_t *proc;
	(void)num_workers;

	proc = alloc_flex(sizeof(*proc), 1, max_block_size);

	if (proc == NULL)
		return NULL;

	proc->max_block_size = max_block_size;
	proc->cmp = cmp;
	proc->cb = callback;
	proc->user = user;
	return proc;
}

void sqfs_block_processor_destroy(sqfs_block_processor_t *proc)
{
	free(proc);
}

int sqfs_block_processor_enqueue(sqfs_block_processor_t *proc,
				 sqfs_block_t *block)
{
	int ret;

	ret = sqfs_block_process(block, proc->cmp,
				 proc->scratch, proc->max_block_size);
	if (ret)
		goto fail;

	ret = proc->cb(proc->user, block);
	if (ret)
		goto fail;

	free(block);
	return 0;
fail:
	free(block);
	proc->status = ret;
	return ret;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	return proc->status;
}
