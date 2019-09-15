/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block_processor.h"
#include "data_writer.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>

#define MK_BLK_SIG(chksum, size) \
	(((uint64_t)(size) << 32) | (uint64_t)(chksum))

#define BLK_SIZE(sig) ((sig) >> 32)

#define INIT_BLOCK_COUNT (128)

typedef struct {
	uint64_t offset;
	uint64_t signature;
} blk_info_t;

typedef struct {
	uint32_t index;
	uint32_t offset;
	uint64_t signature;
} frag_info_t;

struct data_writer_t {
	sqfs_block_t *frag_block;
	sqfs_fragment_t *fragments;
	size_t num_fragments;
	size_t max_fragments;

	size_t devblksz;
	uint64_t start;

	sqfs_block_processor_t *proc;
	sqfs_compressor_t *cmp;
	sqfs_super_t *super;
	sqfs_file_t *file;

	size_t file_start;
	size_t num_blocks;
	size_t max_blocks;
	blk_info_t *blocks;

	size_t frag_list_num;
	size_t frag_list_max;
	frag_info_t *frag_list;

	data_writer_stats_t stats;
};

enum {
	BLK_FIRST_BLOCK = SQFS_BLK_USER,
	BLK_LAST_BLOCK = SQFS_BLK_USER << 1,
	BLK_ALLIGN = SQFS_BLK_USER << 2,
	BLK_FRAGMENT_BLOCK = SQFS_BLK_USER << 3,
};

static int allign_file(data_writer_t *data)
{
	return padd_sqfs(data->file, data->file->get_size(data->file),
			 data->devblksz);
}

static int store_block_location(data_writer_t *data, uint64_t offset,
				uint32_t size, uint32_t chksum)
{
	size_t new_sz;
	void *new;

	if (data->num_blocks == data->max_blocks) {
		new_sz = data->max_blocks * 2;
		new = realloc(data->blocks, sizeof(data->blocks[0]) * new_sz);

		if (new == NULL) {
			perror("growing data block checksum table");
			return -1;
		}

		data->blocks = new;
		data->max_blocks = new_sz;
	}

	data->blocks[data->num_blocks].offset = offset;
	data->blocks[data->num_blocks].signature = MK_BLK_SIG(chksum, size);
	data->num_blocks += 1;
	return 0;
}

static size_t deduplicate_blocks(data_writer_t *data, size_t count)
{
	size_t i, j;

	for (i = 0; i < data->file_start; ++i) {
		for (j = 0; j < count; ++j) {
			if (data->blocks[i + j].signature !=
			    data->blocks[data->file_start + j].signature)
				break;
		}

		if (j == count)
			break;
	}

	return i;
}

static int block_callback(void *user, sqfs_block_t *blk)
{
	file_info_t *fi = blk->user;
	data_writer_t *data = user;
	size_t start, count;
	uint64_t offset;
	uint32_t out;

	if (blk->flags & BLK_FIRST_BLOCK) {
		data->start = data->file->get_size(data->file);
		data->file_start = data->num_blocks;

		if ((blk->flags & BLK_ALLIGN) && allign_file(data) != 0)
			return -1;
	}

	if (blk->size != 0) {
		out = blk->size;
		if (!(blk->flags & SQFS_BLK_IS_COMPRESSED))
			out |= 1 << 24;

		offset = data->file->get_size(data->file);

		if (blk->flags & BLK_FRAGMENT_BLOCK) {
			data->fragments[blk->index].start_offset = htole64(offset);
			data->fragments[blk->index].pad0 = 0;
			data->fragments[blk->index].size = htole32(out);

			data->super->flags &= ~SQFS_FLAG_NO_FRAGMENTS;
			data->super->flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;

			data->stats.frag_blocks_written += 1;
		} else {
			fi->block_size[blk->index] = htole32(out);

			if (store_block_location(data, offset, out,
						 blk->checksum))
				return -1;

			data->stats.blocks_written += 1;
		}

		if (data->file->write_at(data->file, offset,
					 blk->data, blk->size)) {
			return -1;
		}
	}

	if (blk->flags & BLK_LAST_BLOCK) {
		if ((blk->flags & BLK_ALLIGN) && allign_file(data) != 0)
			return -1;

		count = data->num_blocks - data->file_start;
		start = deduplicate_blocks(data, count);

		fi->startblock = data->blocks[start].offset;

		if (start + count < data->file_start) {
			data->num_blocks = data->file_start;

			data->stats.duplicate_blocks += count;

			if (data->file->truncate(data->file, data->start)) {
				perror("truncating squashfs image after "
				       "file deduplication");
				return -1;
			}
		}
	}

	return 0;
}

/*****************************************************************************/

static int flush_fragment_block(data_writer_t *data)
{
	size_t newsz;
	void *new;
	int ret;

	if (data->num_fragments == data->max_fragments) {
		newsz = data->max_fragments ? data->max_fragments * 2 : 16;
		new = realloc(data->fragments,
			      sizeof(data->fragments[0]) * newsz);

		if (new == NULL) {
			perror("appending to fragment table");
			return -1;
		}

		data->max_fragments = newsz;
		data->fragments = new;
	}

	data->frag_block->index = data->num_fragments++;

	ret = sqfs_block_processor_enqueue(data->proc, data->frag_block);
	data->frag_block = NULL;
	return ret;
}

static int handle_fragment(data_writer_t *data, sqfs_block_t *frag)
{
	file_info_t *fi = frag->user;
	size_t i, size, new_sz;
	uint64_t signature;
	void *new;

	frag->checksum = crc32(0, frag->data, frag->size);
	signature = MK_BLK_SIG(frag->checksum, frag->size);

	for (i = 0; i < data->frag_list_num; ++i) {
		if (data->frag_list[i].signature == signature) {
			fi->fragment_offset = data->frag_list[i].offset;
			fi->fragment = data->frag_list[i].index;
			free(frag);

			data->stats.frag_dup += 1;
			return 0;
		}
	}

	if (data->frag_block != NULL) {
		size = data->frag_block->size + frag->size;

		if (size > data->super->block_size) {
			if (flush_fragment_block(data))
				goto fail;
		}
	}

	if (data->frag_block == NULL) {
		size = sizeof(sqfs_block_t) + data->super->block_size;

		data->frag_block = calloc(1, size);
		if (data->frag_block == NULL) {
			perror("creating fragment block");
			goto fail;
		}

		data->frag_block->flags = BLK_FRAGMENT_BLOCK;
	}

	if (data->frag_list_num == data->frag_list_max) {
		new_sz = data->frag_list_max * 2;
		new = realloc(data->frag_list,
			      sizeof(data->frag_list[0]) * new_sz);

		if (new == NULL) {
			perror("growing fragment checksum table");
			return -1;
		}

		data->frag_list = new;
		data->frag_list_max = new_sz;
	}

	data->frag_list[data->frag_list_num].index = data->num_fragments;
	data->frag_list[data->frag_list_num].offset = data->frag_block->size;
	data->frag_list[data->frag_list_num].signature = signature;
	data->frag_list_num += 1;

	fi->fragment_offset = data->frag_block->size;
	fi->fragment = data->num_fragments;

	data->frag_block->flags |= (frag->flags & SQFS_BLK_DONT_COMPRESS);
	memcpy(data->frag_block->data + data->frag_block->size,
	       frag->data, frag->size);

	data->frag_block->size += frag->size;
	free(frag);

	data->stats.frag_count += 1;
	return 0;
fail:
	free(frag);
	return -1;
}

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

static int add_sentinel_block(data_writer_t *data, file_info_t *fi,
			      uint32_t flags)
{
	sqfs_block_t *blk = calloc(1, sizeof(*blk));

	if (blk == NULL) {
		perror("creating sentinel block");
		return -1;
	}

	blk->user = fi;
	blk->flags = SQFS_BLK_DONT_COMPRESS | SQFS_BLK_DONT_CHECKSUM | flags;

	return sqfs_block_processor_enqueue(data->proc, blk);
}

int write_data_from_file_condensed(data_writer_t *data, sqfs_file_t *file,
				   file_info_t *fi,
				   const sqfs_sparse_map_t *map, int flags)
{
	uint32_t blk_flags = BLK_FIRST_BLOCK;
	size_t diff, i = 0;
	sqfs_block_t *blk;
	uint64_t offset;
	int ret;

	if (flags & DW_DONT_COMPRESS)
		blk_flags |= SQFS_BLK_DONT_COMPRESS;

	if (flags & DW_ALLIGN_DEVBLK)
		blk_flags |= BLK_ALLIGN;

	for (offset = 0; offset < fi->size; offset += diff) {
		if (fi->size - offset > (uint64_t)data->super->block_size) {
			diff = data->super->block_size;
		} else {
			diff = fi->size - offset;
		}

		if (map == NULL) {
			ret = sqfs_file_create_block(file, offset, diff, fi,
						     blk_flags, &blk);
		} else {
			ret = sqfs_file_create_block_dense(file, offset, diff,
							   fi, blk_flags,
							   map, &blk);
		}

		if (ret)
			return -1;

		blk->index = i++;

		if (is_zero_block(blk->data, blk->size)) {
			data->stats.sparse_blocks += 1;

			fi->block_size[blk->index] = 0;
			free(blk);
			continue;
		}

		if (diff < data->super->block_size &&
		    !(flags & DW_DONT_FRAGMENT)) {
			if (!(blk_flags & (BLK_FIRST_BLOCK | BLK_LAST_BLOCK))) {
				blk_flags |= BLK_LAST_BLOCK;

				if (add_sentinel_block(data, fi, blk_flags)) {
					free(blk);
					return -1;
				}
			}

			if (handle_fragment(data, blk))
				return -1;
		} else {
			if (sqfs_block_processor_enqueue(data->proc, blk))
				return -1;

			blk_flags &= ~BLK_FIRST_BLOCK;
		}
	}

	if (!(blk_flags & (BLK_FIRST_BLOCK | BLK_LAST_BLOCK))) {
		blk_flags |= BLK_LAST_BLOCK;

		if (add_sentinel_block(data, fi, blk_flags))
			return -1;
	}

	data->stats.bytes_read += fi->size;
	data->stats.file_count += 1;
	return 0;
}

int write_data_from_file(data_writer_t *data, file_info_t *fi,
			 sqfs_file_t *file, int flags)
{
	return write_data_from_file_condensed(data, file, fi, NULL, flags);
}

data_writer_t *data_writer_create(sqfs_super_t *super, sqfs_compressor_t *cmp,
				  sqfs_file_t *file, size_t devblksize,
				  unsigned int num_jobs, size_t max_backlog)
{
	data_writer_t *data = calloc(1, sizeof(*data));

	if (data == NULL) {
		perror("creating data writer");
		return NULL;
	}

	data->max_blocks = INIT_BLOCK_COUNT;
	data->frag_list_max = INIT_BLOCK_COUNT;

	data->blocks = alloc_array(sizeof(data->blocks[0]),
				   data->max_blocks);

	if (data->blocks == NULL) {
		perror("creating data writer");
		free(data);
		return NULL;
	}

	data->frag_list = alloc_array(sizeof(data->frag_list[0]),
				      data->frag_list_max);

	if (data->frag_list == NULL) {
		perror("creating data writer");
		free(data->blocks);
		free(data);
		return NULL;
	}

	data->proc = sqfs_block_processor_create(super->block_size, cmp,
						 num_jobs, max_backlog, data,
						 block_callback);
	if (data->proc == NULL) {
		perror("creating data block processor");
		free(data->frag_list);
		free(data->blocks);
		free(data);
		return NULL;
	}

	data->cmp = cmp;
	data->super = super;
	data->file = file;
	data->devblksz = devblksize;
	return data;
}

void data_writer_destroy(data_writer_t *data)
{
	sqfs_block_processor_destroy(data->proc);
	free(data->fragments);
	free(data->blocks);
	free(data);
}

int data_writer_write_fragment_table(data_writer_t *data)
{
	uint64_t start;
	size_t size;
	int ret;

	if (data->num_fragments == 0) {
		data->super->fragment_entry_count = 0;
		data->super->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
		return 0;
	}

	size = sizeof(data->fragments[0]) * data->num_fragments;
	ret = sqfs_write_table(data->file, data->cmp,
			       data->fragments, size, &start);
	if (ret)
		return -1;

	data->super->fragment_entry_count = data->num_fragments;
	data->super->fragment_table_start = start;
	return 0;
}

int data_writer_sync(data_writer_t *data)
{
	if (data->frag_block != NULL) {
		if (flush_fragment_block(data))
			return -1;
	}

	return sqfs_block_processor_finish(data->proc);
}

data_writer_stats_t *data_writer_get_stats(data_writer_t *data)
{
	return &data->stats;
}
