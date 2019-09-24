/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * deduplicate.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

int store_block_location(sqfs_block_processor_t *proc, uint64_t offset,
			 uint32_t size, uint32_t chksum)
{
	size_t new_sz;
	void *new;

	if (proc->num_blocks == proc->max_blocks) {
		new_sz = proc->max_blocks * 2;
		new = realloc(proc->blocks, sizeof(proc->blocks[0]) * new_sz);

		if (new == NULL)
			return SQFS_ERROR_ALLOC;

		proc->blocks = new;
		proc->max_blocks = new_sz;
	}

	proc->blocks[proc->num_blocks].offset = offset;
	proc->blocks[proc->num_blocks].signature = MK_BLK_SIG(chksum, size);
	proc->num_blocks += 1;
	return 0;
}

size_t deduplicate_blocks(sqfs_block_processor_t *proc, size_t count)
{
	size_t i, j;

	for (i = 0; i < proc->file_start; ++i) {
		for (j = 0; j < count; ++j) {
			if (proc->blocks[i + j].signature !=
			    proc->blocks[proc->file_start + j].signature)
				break;
		}

		if (j == count)
			break;
	}

	return i;
}
