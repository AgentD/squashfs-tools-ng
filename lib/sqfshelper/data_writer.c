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
	uint32_t index;
	uint32_t offset;
	uint64_t signature;
} frag_info_t;

struct data_writer_t {
	sqfs_block_t *frag_block;
	size_t num_fragments;

	sqfs_block_processor_t *proc;
	sqfs_compressor_t *cmp;
	sqfs_super_t *super;

	size_t frag_list_num;
	size_t frag_list_max;
	frag_info_t *frag_list;

	data_writer_stats_t stats;
};

static int flush_fragment_block(data_writer_t *data)
{
	int ret;

	data->frag_block->index = data->num_fragments++;

	ret = sqfs_block_processor_enqueue(data->proc, data->frag_block);
	data->frag_block = NULL;
	return ret;
}

static int handle_fragment(data_writer_t *data, sqfs_block_t *frag)
{
	size_t i, size, new_sz;
	uint64_t signature;
	void *new;

	frag->checksum = crc32(0, frag->data, frag->size);
	signature = MK_BLK_SIG(frag->checksum, frag->size);

	for (i = 0; i < data->frag_list_num; ++i) {
		if (data->frag_list[i].signature == signature) {
			sqfs_inode_set_frag_location(frag->inode,
						     data->frag_list[i].index,
						     data->frag_list[i].offset);
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

		data->frag_block->flags = SQFS_BLK_FRAGMENT_BLOCK;
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

	sqfs_inode_set_frag_location(frag->inode, data->num_fragments,
				     data->frag_block->size);

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

static int add_sentinel_block(data_writer_t *data, sqfs_inode_generic_t *inode,
			      uint32_t flags)
{
	sqfs_block_t *blk = calloc(1, sizeof(*blk));

	if (blk == NULL) {
		perror("creating sentinel block");
		return -1;
	}

	blk->inode = inode;
	blk->flags = SQFS_BLK_DONT_COMPRESS | SQFS_BLK_DONT_CHECKSUM | flags;

	return sqfs_block_processor_enqueue(data->proc, blk);
}

int write_data_from_file_condensed(data_writer_t *data, sqfs_file_t *file,
				   sqfs_inode_generic_t *inode,
				   const sqfs_sparse_map_t *map, int flags)
{
	uint32_t blk_flags = SQFS_BLK_FIRST_BLOCK;
	uint64_t filesz, offset;
	size_t diff, i = 0;
	sqfs_block_t *blk;
	int ret;

	if (flags & DW_DONT_COMPRESS)
		blk_flags |= SQFS_BLK_DONT_COMPRESS;

	if (flags & DW_ALLIGN_DEVBLK)
		blk_flags |= SQFS_BLK_ALLIGN;

	sqfs_inode_get_file_size(inode, &filesz);

	for (offset = 0; offset < filesz; offset += diff) {
		if (filesz - offset > (uint64_t)data->super->block_size) {
			diff = data->super->block_size;
		} else {
			diff = filesz - offset;
		}

		if (map == NULL) {
			ret = sqfs_file_create_block(file, offset, diff, inode,
						     blk_flags, &blk);
		} else {
			ret = sqfs_file_create_block_dense(file, offset, diff,
							   inode, blk_flags,
							   map, &blk);
		}

		if (ret)
			return -1;

		blk->index = i++;

		if (is_zero_block(blk->data, blk->size)) {
			data->stats.sparse_blocks += 1;

			sqfs_inode_make_extended(inode);
			inode->data.file_ext.sparse += blk->size;
			inode->num_file_blocks += 1;

			inode->block_sizes[blk->index] = 0;
			free(blk);
			continue;
		}

		if (diff < data->super->block_size &&
		    !(flags & DW_DONT_FRAGMENT)) {
			if (!(blk_flags & (SQFS_BLK_FIRST_BLOCK |
					   SQFS_BLK_LAST_BLOCK))) {
				blk_flags |= SQFS_BLK_LAST_BLOCK;

				if (add_sentinel_block(data, inode,
						       blk_flags)) {
					free(blk);
					return -1;
				}
			}

			if (handle_fragment(data, blk))
				return -1;
		} else {
			if (sqfs_block_processor_enqueue(data->proc, blk))
				return -1;

			blk_flags &= ~SQFS_BLK_FIRST_BLOCK;
			inode->num_file_blocks += 1;
		}
	}

	if (!(blk_flags & (SQFS_BLK_FIRST_BLOCK | SQFS_BLK_LAST_BLOCK))) {
		blk_flags |= SQFS_BLK_LAST_BLOCK;

		if (add_sentinel_block(data, inode, blk_flags))
			return -1;
	}

	sqfs_inode_make_basic(inode);

	data->stats.bytes_read += filesz;
	data->stats.file_count += 1;
	return 0;
}

int write_data_from_file(data_writer_t *data, sqfs_inode_generic_t *inode,
			 sqfs_file_t *file, int flags)
{
	return write_data_from_file_condensed(data, file, inode, NULL, flags);
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

	data->frag_list_max = INIT_BLOCK_COUNT;
	data->frag_list = alloc_array(sizeof(data->frag_list[0]),
				      data->frag_list_max);
	if (data->frag_list == NULL) {
		perror("creating data writer");
		free(data);
		return NULL;
	}

	data->proc = sqfs_block_processor_create(super->block_size, cmp,
						 num_jobs, max_backlog,
						 devblksize, file);
	if (data->proc == NULL) {
		perror("creating data block processor");
		free(data->frag_list);
		free(data);
		return NULL;
	}

	data->cmp = cmp;
	data->super = super;
	return data;
}

void data_writer_destroy(data_writer_t *data)
{
	sqfs_block_processor_destroy(data->proc);
	free(data->frag_list);
	free(data);
}

int data_writer_write_fragment_table(data_writer_t *data)
{
	if (sqfs_block_processor_write_fragment_table(data->proc,
						      data->super)) {
		fputs("error storing fragment table\n", stderr);
		return -1;
	}
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
