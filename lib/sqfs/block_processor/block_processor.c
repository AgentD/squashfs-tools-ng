/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_processor.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "internal.h"

static int process_block(void *userptr, void *workitem)
{
	worker_data_t *worker = userptr;
	sqfs_block_t *block = workitem;
	sqfs_s32 ret;

	if (block->size == 0)
		return 0;

	if (!(block->flags & SQFS_BLK_IGNORE_SPARSE) &&
	    is_memory_zero(block->data, block->size)) {
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

	ret = worker->cmp->do_block(worker->cmp, block->data, block->size,
				    worker->scratch, worker->scratch_size);
	if (ret < 0)
		return ret;

	if (ret > 0) {
		memcpy(block->data, worker->scratch, ret);
		block->size = ret;
		block->flags |= SQFS_BLK_IS_COMPRESSED;
	}
	return 0;
}

static bool chunk_info_equals(void *user, const void *k, const void *c)
{
	const chunk_info_t *key = k, *cmp = c;
	(void)user;
	return key->size == cmp->size && key->hash == cmp->hash;
}

static void ht_delete_function(struct hash_entry *entry)
{
	free(entry->data);
}

static void block_processor_destroy(sqfs_object_t *base)
{
	sqfs_block_processor_t *proc = (sqfs_block_processor_t *)base;
	sqfs_block_t *it;

	free(proc->frag_block);
	free(proc->blk_current);

	while (proc->free_list != NULL) {
		it = proc->free_list;
		proc->free_list = it->next;
		free(it);
	}

	while (proc->io_queue != NULL) {
		it = proc->io_queue;
		proc->io_queue = it->next;
		free(it);
	}

	if (proc->frag_ht != NULL)
		hash_table_destroy(proc->frag_ht, ht_delete_function);

	/* XXX: shut down the pool first before cleaning up the worker data */
	if (proc->pool != NULL)
		proc->pool->destroy(proc->pool);

	while (proc->workers != NULL) {
		worker_data_t *worker = proc->workers;
		proc->workers = worker->next;

		sqfs_destroy(worker->cmp);
		free(worker);
	}

	free(proc);
}

int sqfs_block_processor_sync(sqfs_block_processor_t *proc)
{
	int ret;

	for (;;) {
		if (proc->backlog == 0)
			break;

		if ((proc->backlog == 1) &&
		    (proc->frag_block != NULL || proc->blk_current != NULL)) {
			break;
		}

		if ((proc->backlog == 2) &&
		    proc->frag_block != NULL && proc->blk_current != NULL) {
			break;
		}

		ret = dequeue_block(proc);
		if (ret != 0)
			return ret;
	}

	return 0;
}

int sqfs_block_processor_finish(sqfs_block_processor_t *proc)
{
	sqfs_block_t *blk;
	int status;

	status = sqfs_block_processor_sync(proc);
	if (status != 0)
		return status;

	if (proc->frag_block != NULL) {
		blk = proc->frag_block;
		blk->next = NULL;
		proc->frag_block = NULL;

		blk->io_seq_num = proc->io_seq_num++;

		status = enqueue_block(proc, blk);
		if (status != 0)
			return status;

		status = sqfs_block_processor_sync(proc);
	}

	return status;
}

const sqfs_block_processor_stats_t
*sqfs_block_processor_get_stats(const sqfs_block_processor_t *proc)
{
	return &proc->stats;
}

int sqfs_block_processor_create_ex(const sqfs_block_processor_desc_t *desc,
				   sqfs_block_processor_t **out)
{
	sqfs_block_processor_t *proc;
	size_t i, count;
	int ret;

	if (desc->size != sizeof(sqfs_block_processor_desc_t))
		return SQFS_ERROR_ARG_INVALID;

	proc = calloc(1, sizeof(*proc));
	if (proc == NULL)
		return SQFS_ERROR_ALLOC;

	proc->max_backlog = desc->max_backlog;
	proc->max_block_size = desc->max_block_size;
	proc->frag_tbl = desc->tbl;
	proc->wr = desc->wr;
	proc->file = desc->file;
	proc->uncmp = desc->uncmp;
	proc->stats.size = sizeof(proc->stats);
	((sqfs_object_t *)proc)->destroy = block_processor_destroy;

	/* we need at least one current data block + one fragment block */
	if (proc->max_backlog < 2)
		proc->max_backlog = 2;

	/* create the thread pool */
	proc->pool = thread_pool_create(desc->num_workers, process_block);
	if (proc->pool == NULL) {
		free(proc);
		return SQFS_ERROR_INTERNAL;
	}

	/* create the worker compressors & scratch buffer */
	count = proc->pool->get_worker_count(proc->pool);

	for (i = 0; i < count; ++i) {
		worker_data_t *worker = alloc_flex(sizeof(*worker), 1,
						   desc->max_block_size);
		if (worker == NULL) {
			ret = SQFS_ERROR_ALLOC;
			goto fail_pool;
		}

		worker->scratch_size = desc->max_block_size;
		worker->next = proc->workers;
		proc->workers = worker;

		worker->cmp = sqfs_copy(desc->cmp);
		if (worker->cmp == NULL) {
			ret = SQFS_ERROR_ALLOC;
			goto fail_pool;
		}

		proc->pool->set_worker_ptr(proc->pool, i, worker);
	}

	/* create the fragment hash table */
	proc->frag_ht = hash_table_create(NULL, chunk_info_equals);
	if (proc->frag_ht == NULL) {
		ret = SQFS_ERROR_ALLOC;
		goto fail_pool;
	}

	proc->frag_ht->user = proc;
	*out = proc;
	return 0;
fail_pool:
	block_processor_destroy((sqfs_object_t *)proc);
	return ret;
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
