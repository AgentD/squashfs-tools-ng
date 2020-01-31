/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * process_block.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

#include <string.h>

int process_completed_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_u64 location;
	sqfs_u32 size;
	int err;

	size = blk->size;
	if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
		size |= 1 << 24;

	err = sqfs_block_writer_write(proc->wr, blk, &location);
	if (err)
		return err;

	if (blk->size != 0) {
		if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
			err = sqfs_frag_table_set(proc->frag_tbl, blk->index,
						  location, size);
			if (err)
				return err;
		} else {
			blk->inode->extra[blk->index] = size;
		}
	}

	if (blk->flags & SQFS_BLK_LAST_BLOCK)
		sqfs_inode_set_file_block_start(blk->inode, location);

	return 0;
}

int block_processor_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			     sqfs_u8 *scratch, size_t scratch_size)
{
	ssize_t ret;

	if (block->size == 0) {
		block->checksum = 0;
		return 0;
	}

	block->checksum = crc32(0, block->data, block->size);

	if (block->flags & SQFS_BLK_IS_FRAGMENT)
		return 0;

	if (!(block->flags & SQFS_BLK_DONT_COMPRESS)) {
		ret = cmp->do_block(cmp, block->data, block->size,
				    scratch, scratch_size);
		if (ret < 0)
			return ret;

		if (ret > 0) {
			memcpy(block->data, scratch, ret);
			block->size = ret;
			block->flags |= SQFS_BLK_IS_COMPRESSED;
		}
	}

	return 0;
}
