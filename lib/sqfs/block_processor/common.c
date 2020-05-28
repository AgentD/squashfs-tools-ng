/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * common.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static int set_block_size(sqfs_inode_generic_t **inode,
			  sqfs_u32 index, sqfs_u32 size)
{
	size_t min_size = (index + 1) * sizeof(sqfs_u32);
	size_t avail = (*inode)->payload_bytes_available;
	size_t newsz;
	void *new;

	if (avail < min_size) {
		newsz = avail ? avail : (sizeof(sqfs_u32) * 4);
		while (newsz < min_size)
			newsz *= 2;

		if (SZ_ADD_OV(newsz, sizeof(**inode), &newsz))
			return SQFS_ERROR_OVERFLOW;

		if (sizeof(size_t) > sizeof(sqfs_u32)) {
			if ((newsz - sizeof(**inode)) > 0x0FFFFFFFFUL)
				return SQFS_ERROR_OVERFLOW;
		}

		new = realloc((*inode), newsz);
		if (new == NULL)
			return SQFS_ERROR_ALLOC;

		(*inode) = new;
		(*inode)->payload_bytes_available = newsz - sizeof(**inode);
	}

	(*inode)->extra[index] = size;

	if (min_size >= (*inode)->payload_bytes_used)
		(*inode)->payload_bytes_used = min_size;

	return 0;
}

static void release_old_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_block_t *it;

	if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK && blk->frag_list != NULL) {
		it = blk->frag_list;

		while (it->next != NULL)
			it = it->next;

		it->next = proc->free_list;
		proc->free_list = blk->frag_list;
	}

	blk->next = proc->free_list;
	proc->free_list = blk;
}

int process_completed_block(sqfs_block_processor_t *proc, sqfs_block_t *blk)
{
	sqfs_u64 location;
	sqfs_u32 size;
	int err;

	err = proc->wr->write_data_block(proc->wr, blk->size, blk->checksum,
					 blk->flags, blk->data, &location);
	if (err)
		goto out;

	proc->stats.output_bytes_generated += blk->size;

	if (blk->flags & SQFS_BLK_IS_SPARSE) {
		sqfs_inode_make_extended(*(blk->inode));
		(*(blk->inode))->data.file_ext.sparse += blk->size;

		err = set_block_size(blk->inode, blk->index, 0);
		if (err)
			goto out;

		proc->stats.sparse_block_count += 1;
	} else if (blk->size != 0) {
		size = blk->size;
		if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
			size |= 1 << 24;

		if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
			err = sqfs_frag_table_set(proc->frag_tbl, blk->index,
						  location, size);
			if (err)
				goto out;
			proc->stats.frag_block_count += 1;
		} else {
			err = set_block_size(blk->inode, blk->index, size);
			if (err)
				goto out;
			proc->stats.data_block_count += 1;
		}
	}

	if (blk->flags & SQFS_BLK_LAST_BLOCK)
		sqfs_inode_set_file_block_start(*(blk->inode), location);
out:
	release_old_block(proc, blk);
	return err;
}

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

int block_processor_do_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			     sqfs_u8 *scratch, size_t scratch_size)
{
	sqfs_block_t *it;
	size_t offset;
	sqfs_s32 ret;

	if (block->size == 0)
		return 0;

	if (block->flags & SQFS_BLK_FRAGMENT_BLOCK) {
		offset = block->size;

		for (it = block->frag_list; it != NULL; it = it->next) {
			assert(offset >= it->size);
			offset -= it->size;

			memcpy(block->data + offset, it->data, it->size);

			block->flags |= (it->flags & SQFS_BLK_DONT_COMPRESS);
		}
	}

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
		sqfs_inode_make_extended(*(frag->inode));
		set_block_size(frag->inode, frag->index, 0);
		(*(frag->inode))->data.file_ext.sparse += frag->size;

		proc->stats.sparse_block_count += 1;
		release_old_block(proc, frag);
		return 0;
	}

	proc->stats.total_frag_count += 1;

	if (!(frag->flags & SQFS_BLK_DONT_DEDUPLICATE)) {
		err = sqfs_frag_table_find_tail_end(proc->frag_tbl,
						    frag->checksum, frag->size,
						    &index, &offset);
		if (err == 0) {
			sqfs_inode_set_frag_location(*(frag->inode),
						     index, offset);
			release_old_block(proc, frag);
			return 0;
		}
	}

	if (proc->frag_block != NULL) {
		size = proc->frag_block->size + frag->size;

		if (size > proc->max_block_size) {
			*blk_out = proc->frag_block;
			proc->frag_block = NULL;
		}
	}

	if (proc->frag_block == NULL) {
		err = sqfs_frag_table_append(proc->frag_tbl, 0, 0, &index);
		if (err)
			goto fail;

		offset = 0;
		proc->frag_block = frag;
		proc->frag_block->index = index;
		proc->frag_block->flags &= SQFS_BLK_DONT_COMPRESS;
		proc->frag_block->flags |= SQFS_BLK_FRAGMENT_BLOCK;
		proc->frag_block->frag_list = NULL;
	} else {
		index = proc->frag_block->index;
		offset = proc->frag_block->size;

		frag->next = proc->frag_block->frag_list;
		proc->frag_block->frag_list = frag;

		proc->frag_block->size += frag->size;
	}

	err = sqfs_frag_table_add_tail_end(proc->frag_tbl, index, offset,
					   frag->size, frag->checksum);
	if (err)
		goto fail_outblk;

	sqfs_inode_set_frag_location(*(frag->inode), index, offset);
	proc->stats.actual_frag_count += 1;
	return 0;
fail:
	release_old_block(proc, frag);
fail_outblk:
	if (*blk_out != NULL) {
		release_old_block(proc, *blk_out);
		*blk_out = NULL;
	}
	return err;
}
