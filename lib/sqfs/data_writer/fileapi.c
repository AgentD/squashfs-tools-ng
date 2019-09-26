/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fileapi.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

static int add_sentinel_block(sqfs_data_writer_t *proc)
{
	sqfs_block_t *blk = calloc(1, sizeof(*blk));

	if (blk == NULL)
		return test_and_set_status(proc, SQFS_ERROR_ALLOC);

	blk->inode = proc->inode;
	blk->flags = proc->blk_flags | SQFS_BLK_LAST_BLOCK;

	return data_writer_enqueue(proc, blk);
}

int sqfs_data_writer_begin_file(sqfs_data_writer_t *proc,
				sqfs_inode_generic_t *inode, uint32_t flags)
{
	if (proc->inode != NULL)
		return test_and_set_status(proc, SQFS_ERROR_INTERNAL);

	if (flags & ~SQFS_BLK_USER_SETTABLE_FLAGS)
		return test_and_set_status(proc, SQFS_ERROR_UNSUPPORTED);

	proc->inode = inode;
	proc->blk_flags = flags | SQFS_BLK_FIRST_BLOCK;
	proc->blk_index = 0;
	proc->blk_current = NULL;
	return 0;
}

static int flush_block(sqfs_data_writer_t *proc, sqfs_block_t *block)
{
	block->index = proc->blk_index++;
	block->flags = proc->blk_flags;
	block->inode = proc->inode;

	if (is_zero_block(block->data, block->size)) {
		sqfs_inode_make_extended(proc->inode);
		proc->inode->data.file_ext.sparse += block->size;
		proc->inode->num_file_blocks += 1;
		proc->inode->block_sizes[block->index] = 0;
		free(block);
		return 0;
	}

	if (block->size < proc->max_block_size) {
		block->flags |= SQFS_BLK_IS_FRAGMENT;
	} else {
		proc->inode->num_file_blocks += 1;
		proc->blk_flags &= ~SQFS_BLK_FIRST_BLOCK;
	}

	return data_writer_enqueue(proc, block);
}

int sqfs_data_writer_append(sqfs_data_writer_t *proc, const void *data,
			    size_t size)
{
	size_t diff;
	void *new;
	int err;

	while (size > 0) {
		if (proc->blk_current == NULL) {
			new = alloc_flex(sizeof(*proc->blk_current), 1,
					 proc->max_block_size);

			if (new == NULL)
				return test_and_set_status(proc,
							   SQFS_ERROR_ALLOC);

			proc->blk_current = new;
		}

		diff = proc->max_block_size - proc->blk_current->size;

		if (diff == 0) {
			err = flush_block(proc, proc->blk_current);
			proc->blk_current = NULL;
			if (err)
				return err;
			continue;
		}

		if (diff > size)
			diff = size;

		memcpy(proc->blk_current->data + proc->blk_current->size,
		       data, diff);

		size -= diff;
		proc->blk_current->size += diff;
		data = (const char *)data + diff;
	}

	if (proc->blk_current != NULL &&
	    proc->blk_current->size == proc->max_block_size) {
		err = flush_block(proc, proc->blk_current);
		proc->blk_current = NULL;
		return err;
	}

	return 0;
}

int sqfs_data_writer_end_file(sqfs_data_writer_t *proc)
{
	int err;

	if (proc->inode == NULL)
		return test_and_set_status(proc, SQFS_ERROR_INTERNAL);

	if (!(proc->blk_flags & SQFS_BLK_FIRST_BLOCK)) {
		err = add_sentinel_block(proc);
		if (err)
			return err;
	}

	if (proc->blk_current != NULL) {
		err = flush_block(proc, proc->blk_current);
		proc->blk_current = NULL;
	}

	proc->inode = NULL;
	proc->blk_flags = 0;
	proc->blk_index = 0;
	return 0;
}
