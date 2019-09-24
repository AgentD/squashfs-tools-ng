/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * process_block.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

#include <string.h>

int sqfs_block_process(sqfs_block_t *block, sqfs_compressor_t *cmp,
		       uint8_t *scratch, size_t scratch_size)
{
	ssize_t ret;

	if (block->size == 0) {
		block->checksum = 0;
		return 0;
	}

	block->checksum = crc32(0, block->data, block->size);

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

static int allign_file(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	if (!(blk->flags & SQFS_BLK_ALLIGN))
		return 0;

	return padd_sqfs(proc->file, proc->file->get_size(proc->file),
			 proc->devblksz);
}

int process_completed_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	size_t start, count;
	uint64_t offset;
	uint32_t out;
	int err;

	if (blk->flags & SQFS_BLK_FIRST_BLOCK) {
		proc->start = proc->file->get_size(proc->file);
		proc->file_start = proc->num_blocks;

		err = allign_file(proc, blk);
		if (err)
			return err;
	}

	if (blk->size != 0) {
		out = blk->size;
		if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
			out |= 1 << 24;

		offset = proc->file->get_size(proc->file);

		if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
			offset = htole64(offset);
			proc->fragments[blk->index].start_offset = offset;
			proc->fragments[blk->index].pad0 = 0;
			proc->fragments[blk->index].size = htole32(out);
		} else {
			blk->inode->block_sizes[blk->index] = out;
		}

		err = store_block_location(proc, offset, out, blk->checksum);
		if (err)
			return err;

		err = proc->file->write_at(proc->file, offset,
					   blk->data, blk->size);
		if (err)
			return err;
	}

	if (blk->flags & SQFS_BLK_LAST_BLOCK) {
		err = allign_file(proc, blk);
		if (err)
			return err;

		count = proc->num_blocks - proc->file_start;
		start = deduplicate_blocks(proc, count);
		offset = proc->blocks[start].offset;

		sqfs_inode_set_file_block_start(blk->inode, offset);

		if (start < proc->file_start) {
			offset = start + count;

			if (offset >= proc->file_start) {
				proc->num_blocks = offset;
			} else {
				proc->num_blocks = proc->file_start;
			}

			err = proc->file->truncate(proc->file, proc->start);
			if (err)
				return err;
		}
	}

	return 0;
}
