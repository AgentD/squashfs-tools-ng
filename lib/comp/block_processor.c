/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "block_processor.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

struct block_processor_t {
	size_t max_block_size;
	compressor_t *cmp;
	block_cb cb;
	void *user;
	int status;

	uint8_t scratch[];
};

block_processor_t *block_processor_create(size_t max_block_size,
					  compressor_t *cmp,
					  unsigned int num_workers,
					  void *user,
					  block_cb callback)
{
	block_processor_t *proc = calloc(1, sizeof(*proc) + max_block_size);
	(void)num_workers;

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

void block_processor_destroy(block_processor_t *proc)
{
	free(proc);
}

int block_processor_enqueue(block_processor_t *proc, block_t *block)
{
	if (process_block(block, proc->cmp,
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

int block_processor_finish(block_processor_t *proc)
{
	return proc->status;
}
