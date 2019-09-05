/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block_processor.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

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

	if (proc == NULL) {
		perror("Creating block processor");
		return NULL;
	}

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
	if (sqfs_block_process(block, proc->cmp,
			       proc->scratch, proc->max_block_size))
		goto fail;

	if (proc->cb(proc->user, block))
		goto fail;

	free(block);
	return 0;
fail:
	free(block);
	proc->status = -1;
	return -1;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	return proc->status;
}
