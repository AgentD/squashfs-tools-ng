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
