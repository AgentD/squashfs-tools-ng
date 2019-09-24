/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fragtbl.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

int sqfs_block_processor_write_fragment_table(sqfs_block_processor_t *proc,
					      sqfs_super_t *super)
{
	uint64_t start;
	size_t size;
	int ret;

	if (proc->num_fragments == 0) {
		super->fragment_entry_count = 0;
		super->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
		super->flags &= ~SQFS_FLAG_ALWAYS_FRAGMENTS;
		super->flags |= SQFS_FLAG_NO_FRAGMENTS;
		return 0;
	}

	size = sizeof(proc->fragments[0]) * proc->num_fragments;
	ret = sqfs_write_table(proc->file, proc->cmp,
			       proc->fragments, size, &start);
	if (ret)
		return ret;

	super->flags &= ~SQFS_FLAG_NO_FRAGMENTS;
	super->flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	super->fragment_entry_count = proc->num_fragments;
	super->fragment_table_start = start;
	return 0;
}

int grow_fragment_table(sqfs_block_processor_t *proc, size_t index)
{
	size_t newsz;
	void *new;

	if (index < proc->max_fragments)
		return 0;

	do {
		newsz = proc->max_fragments ? proc->max_fragments * 2 : 16;
	} while (index >= newsz);

	new = realloc(proc->fragments, sizeof(proc->fragments[0]) * newsz);

	if (new == NULL)
		return SQFS_ERROR_ALLOC;

	proc->max_fragments = newsz;
	proc->fragments = new;
	return 0;
}
