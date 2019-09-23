/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * process_block.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

#include <string.h>
#include <zlib.h>

int sqfs_block_process(sqfs_block_t *block, sqfs_compressor_t *cmp,
		       uint8_t *scratch, size_t scratch_size)
{
	ssize_t ret;

	if (!(block->flags & SQFS_BLK_DONT_CHECKSUM))
		block->checksum = crc32(0, block->data, block->size);

	if (!(block->flags & SQFS_BLK_DONT_COMPRESS)) {
		ret = cmp->do_block(cmp, block->data, block->size,
				    scratch, scratch_size);

		if (ret < 0) {
			block->flags |= SQFS_BLK_COMPRESS_ERROR;
			return ret;
		}

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

static int store_block_location(sqfs_block_processor_t *proc, uint64_t offset,
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

static size_t deduplicate_blocks(sqfs_block_processor_t *proc, size_t count)
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

static size_t grow_fragment_table(sqfs_block_processor_t *proc, size_t index)
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

static int handle_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
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
			if (grow_fragment_table(proc, blk->index))
				return 0;

			offset = htole64(offset);
			proc->fragments[blk->index].start_offset = offset;
			proc->fragments[blk->index].pad0 = 0;
			proc->fragments[blk->index].size = htole32(out);

			if (blk->index >= proc->num_fragments)
				proc->num_fragments = blk->index + 1;
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

int process_completed_blocks(sqfs_block_processor_t *proc, sqfs_block_t *queue)
{
	sqfs_block_t *it;

	while (queue != NULL) {
		it = queue;
		queue = queue->next;

		if (it->flags & SQFS_BLK_COMPRESS_ERROR) {
			proc->status = SQFS_ERROR_COMRPESSOR;
		} else if (proc->status == 0) {
			proc->status = handle_block(proc, it);
		}

		free(it);
	}

	return proc->status;
}
