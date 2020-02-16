/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * common.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

int process_completed_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_u64 location;
	sqfs_u32 size;
	int err;

	err = sqfs_block_writer_write(proc->wr, blk->size, blk->checksum,
				      blk->flags, blk->data, &location);
	if (err)
		return err;

	if (blk->flags & SQFS_BLK_IS_SPARSE) {
		sqfs_inode_make_extended(blk->inode);
		blk->inode->data.file_ext.sparse += blk->size;
		blk->inode->extra[blk->inode->num_file_blocks] = 0;
		blk->inode->num_file_blocks += 1;

		proc->stats.sparse_block_count += 1;
	} else if (blk->size != 0) {
		size = blk->size;
		if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
			size |= 1 << 24;

		if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
			err = sqfs_frag_table_set(proc->frag_tbl, blk->index,
						  location, size);
			if (err)
				return err;
		} else {
			blk->inode->extra[blk->inode->num_file_blocks] = size;
			blk->inode->num_file_blocks += 1;

			proc->stats.data_block_count += 1;
		}
	}

	if (blk->flags & SQFS_BLK_LAST_BLOCK)
		sqfs_inode_set_file_block_start(blk->inode, location);

	return 0;
}

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

int block_processor_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			     sqfs_u8 *scratch, size_t scratch_size)
{
	sqfs_s32 ret;

	if (block->size == 0)
		return 0;

	if (is_zero_block(block->data, block->size)) {
		block->flags |= SQFS_BLK_IS_SPARSE;
		return 0;
	}

	block->checksum = xxh32(block->data, block->size);

	if (block->flags & (SQFS_BLK_IS_FRAGMENT | SQFS_BLK_DONT_COMPRESS))
		return 0;

	ret = cmp->do_block(cmp, block->data, block->size,
			    scratch, scratch_size);
	if (ret < 0)
		return ret;

	if (ret > 0) {
		memcpy(block->data, scratch, ret);
		block->size = ret;
		block->flags |= SQFS_BLK_IS_COMPRESSED;
	}
	return 0;
}

int process_completed_fragment(sqfs_block_processor_t *proc, sqfs_block_t *frag,
			       sqfs_block_t **blk_out)
{
	sqfs_u32 index, offset;
	size_t size;
	int err;

	if (frag->flags & SQFS_BLK_IS_SPARSE) {
		sqfs_inode_make_extended(frag->inode);
		frag->inode->data.file_ext.sparse += frag->size;
		frag->inode->extra[frag->inode->num_file_blocks] = 0;
		frag->inode->num_file_blocks += 1;

		proc->stats.sparse_block_count += 1;
		return 0;
	}

	proc->stats.total_frag_count += 1;

	err = sqfs_frag_table_find_tail_end(proc->frag_tbl, frag->checksum,
					    frag->size, &index, &offset);
	if (err == 0) {
		sqfs_inode_set_frag_location(frag->inode, index, offset);
		return 0;
	}

	if (proc->frag_block != NULL) {
		size = proc->frag_block->size + frag->size;

		if (size > proc->max_block_size) {
			*blk_out = proc->frag_block;
			proc->frag_block = NULL;
		}
	}

	if (proc->frag_block == NULL) {
		size = sizeof(sqfs_block_t) + proc->max_block_size;

		err= sqfs_frag_table_append(proc->frag_tbl, 0, 0, &index);
		if (err)
			goto fail;

		proc->frag_block = calloc(1, size);
		if (proc->frag_block == NULL) {
			err = SQFS_ERROR_ALLOC;
			goto fail;
		}

		proc->frag_block->index = index;
		proc->frag_block->flags = SQFS_BLK_FRAGMENT_BLOCK;
		proc->stats.frag_block_count += 1;
	}

	err = sqfs_frag_table_add_tail_end(proc->frag_tbl,
					   proc->frag_block->index,
					   proc->frag_block->size,
					   frag->size, frag->checksum);
	if (err)
		goto fail;

	sqfs_inode_set_frag_location(frag->inode, proc->frag_block->index,
				     proc->frag_block->size);

	memcpy(proc->frag_block->data + proc->frag_block->size,
	       frag->data, frag->size);

	proc->frag_block->flags |= (frag->flags & SQFS_BLK_DONT_COMPRESS);
	proc->frag_block->size += frag->size;
	proc->stats.actual_frag_count += 1;
	return 0;
fail:
	free(*blk_out);
	*blk_out = NULL;
	return err;
}

const sqfs_block_processor_stats_t
*sqfs_block_processor_get_stats(const sqfs_block_processor_t *proc)
{
	return &proc->stats;
}
