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
	blk->next = proc->free_list;
	proc->free_list = blk;
}

static int process_completed_block(sqfs_block_processor_t *proc,
				   sqfs_block_t *blk)
{
	sqfs_u64 location;
	sqfs_u32 size;
	int err;

	err = proc->wr->write_data_block(proc->wr, blk->user, blk->size,
					 blk->checksum,
					 blk->flags & ~BLK_FLAG_INTERNAL,
					 blk->data, &location);
	if (err)
		goto out;

	proc->stats.output_bytes_generated += blk->size;

	if (blk->flags & SQFS_BLK_IS_SPARSE) {
		if (blk->inode != NULL) {
			sqfs_inode_make_extended(*(blk->inode));
			(*(blk->inode))->data.file_ext.sparse += blk->size;

			err = set_block_size(blk->inode, blk->index, 0);
			if (err)
				goto out;
		}
		proc->stats.sparse_block_count += 1;
	} else if (blk->size != 0) {
		size = blk->size;
		if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
			size |= 1 << 24;

		if (blk->flags & SQFS_BLK_FRAGMENT_BLOCK) {
			if (proc->frag_tbl != NULL) {
				err = sqfs_frag_table_set(proc->frag_tbl,
							  blk->index, location,
							  size);
				if (err)
					goto out;
			}
			proc->stats.frag_block_count += 1;
		} else {
			if (blk->inode != NULL) {
				err = set_block_size(blk->inode, blk->index,
						     size);
				if (err)
					goto out;
			}
			proc->stats.data_block_count += 1;
		}
	}

	if (blk->flags & SQFS_BLK_LAST_BLOCK && blk->inode != NULL)
		sqfs_inode_set_file_block_start(*(blk->inode), location);
out:
	release_old_block(proc, blk);
	return err;
}

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

static int process_block(sqfs_block_t *block, sqfs_compressor_t *cmp,
			 sqfs_u8 *scratch, size_t scratch_size)
{
	sqfs_s32 ret;

	if (block->size == 0)
		return 0;

	if (!(block->flags & SQFS_BLK_IGNORE_SPARSE) &&
	    is_zero_block(block->data, block->size)) {
		block->flags |= SQFS_BLK_IS_SPARSE;
		return 0;
	}

	if (block->flags & SQFS_BLK_DONT_HASH) {
		block->checksum = 0;
	} else {
		block->checksum = xxh32(block->data, block->size);
	}

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

static int process_completed_fragment(sqfs_block_processor_t *proc,
				      sqfs_block_t *frag,
				      sqfs_block_t **blk_out)
{
	chunk_info_t *chunk, search;
	struct hash_entry *entry;
	sqfs_u32 index, offset;
	size_t size;
	int err;

	if (frag->flags & SQFS_BLK_IS_SPARSE) {
		if (frag->inode != NULL) {
			sqfs_inode_make_extended(*(frag->inode));
			set_block_size(frag->inode, frag->index, 0);
			(*(frag->inode))->data.file_ext.sparse += frag->size;
		}
		proc->stats.sparse_block_count += 1;
		release_old_block(proc, frag);
		return 0;
	}

	proc->stats.total_frag_count += 1;

	if (!(frag->flags & SQFS_BLK_DONT_DEDUPLICATE)) {
		search.hash = frag->checksum;
		search.size = frag->size;

		entry = hash_table_search_pre_hashed(proc->frag_ht,
						     search.hash, &search);
		if (entry != NULL) {
			if (frag->inode != NULL) {
				chunk = entry->data;
				sqfs_inode_set_frag_location(*(frag->inode),
							     chunk->index,
							     chunk->offset);
			}
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
		if (proc->frag_tbl == NULL) {
			index = 0;
		} else {
			err = sqfs_frag_table_append(proc->frag_tbl,
						     0, 0, &index);
			if (err)
				goto fail;
		}

		offset = 0;
		proc->frag_block = frag;
		proc->frag_block->index = index;
		proc->frag_block->flags &= SQFS_BLK_DONT_COMPRESS;
		proc->frag_block->flags |= SQFS_BLK_FRAGMENT_BLOCK;
	} else {
		index = proc->frag_block->index;
		offset = proc->frag_block->size;

		memcpy(proc->frag_block->data + proc->frag_block->size,
		       frag->data, frag->size);

		proc->frag_block->size += frag->size;
		proc->frag_block->flags |=
			(frag->flags & SQFS_BLK_DONT_COMPRESS);
		release_old_block(proc, frag);
	}

	if (proc->frag_tbl != NULL) {
		err = SQFS_ERROR_ALLOC;
		chunk = calloc(1, sizeof(*chunk));
		if (chunk == NULL)
			goto fail_outblk;

		chunk->index = index;
		chunk->offset = offset;
		chunk->size = frag->size;
		chunk->hash = frag->checksum;

		entry = hash_table_insert_pre_hashed(proc->frag_ht, chunk->hash,
						     chunk, chunk);
		if (entry == NULL) {
			free(chunk);
			goto fail_outblk;
		}
	}

	if (frag->inode != NULL)
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

static uint32_t chunk_info_hash(const void *key)
{
	const chunk_info_t *chunk = key;
	return chunk->hash;
}

static bool chunk_info_equals(const void *a, const void *b)
{
	const chunk_info_t *a_ = a, *b_ = b;
	return a_->size == b_->size &&
	       a_->hash == b_->hash;
}

static void ht_delete_function(struct hash_entry *entry)
{
	free(entry->data);
}

void block_processor_cleanup(sqfs_block_processor_t *base)
{
	sqfs_block_t *it;

	if (base->frag_block != NULL)
		release_old_block(base, base->frag_block);

	free(base->blk_current);

	while (base->free_list != NULL) {
		it = base->free_list;
		base->free_list = it->next;
		free(it);
	}

	hash_table_destroy(base->frag_ht, ht_delete_function);
}

int block_processor_init(sqfs_block_processor_t *base,
			 const sqfs_block_processor_desc_t *desc)
{
	base->process_completed_block = process_completed_block;
	base->process_completed_fragment = process_completed_fragment;
	base->process_block = process_block;
	base->max_block_size = desc->max_block_size;
	base->cmp = desc->cmp;
	base->frag_tbl = desc->tbl;
	base->wr = desc->wr;
	base->file = desc->file;
	base->uncmp = desc->uncmp;
	base->stats.size = sizeof(base->stats);

	base->frag_ht = hash_table_create(chunk_info_hash, chunk_info_equals);
	if (base->frag_ht == NULL)
		return SQFS_ERROR_ALLOC;

	return 0;
}

sqfs_block_processor_t *sqfs_block_processor_create(size_t max_block_size,
						    sqfs_compressor_t *cmp,
						    unsigned int num_workers,
						    size_t max_backlog,
						    sqfs_block_writer_t *wr,
						    sqfs_frag_table_t *tbl)
{
	sqfs_block_processor_desc_t desc;
	sqfs_block_processor_t *out;

	memset(&desc, 0, sizeof(desc));
	desc.size = sizeof(desc);
	desc.max_block_size = max_block_size;
	desc.num_workers = num_workers;
	desc.max_backlog = max_backlog;
	desc.cmp = cmp;
	desc.wr = wr;
	desc.tbl = tbl;

	if (sqfs_block_processor_create_ex(&desc, &out) != 0)
		return NULL;

	return out;
}
