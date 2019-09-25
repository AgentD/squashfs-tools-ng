/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/data_writer.h"
#include "sqfs/block.h"

#include "data_writer.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>

struct data_writer_t {
	sqfs_data_writer_t *proc;
	sqfs_compressor_t *cmp;
	sqfs_super_t *super;

	data_writer_stats_t stats;
};


static void post_block_write(void *user, const sqfs_block_t *block,
			     sqfs_file_t *file)
{
	data_writer_t *data = user;
	(void)file;

	if (block->flags & SQFS_BLK_FRAGMENT_BLOCK) {
		data->stats.frag_blocks_written += 1;
	} else {
		data->stats.blocks_written += 1;
	}

	data->stats.bytes_written += block->size;
}

static void pre_fragment_store(void *user, sqfs_block_t *block)
{
	data_writer_t *data = user;
	(void)block;

	data->stats.frag_count += 1;
}

static void notify_blocks_erased(void *user, size_t count, uint64_t bytes)
{
	data_writer_t *data = user;
	(void)bytes;

	data->stats.bytes_written -= bytes;
	data->stats.blocks_written -= count;
	data->stats.duplicate_blocks += count;
}

static void notify_fragment_discard(void *user, const sqfs_block_t *block)
{
	data_writer_t *data = user;
	(void)block;

	data->stats.frag_dup += 1;
}

static const sqfs_block_hooks_t hooks = {
	.post_block_write = post_block_write,
	.pre_fragment_store = pre_fragment_store,
	.notify_blocks_erased = notify_blocks_erased,
	.notify_fragment_discard = notify_fragment_discard,
};

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
	blk->flags = flags;

	return sqfs_data_writer_enqueue(data->proc, blk);
}

int write_data_from_file(data_writer_t *data, sqfs_inode_generic_t *inode,
			 sqfs_file_t *file, int flags)
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

		ret = sqfs_file_create_block(file, offset, diff, inode,
					     blk_flags, &blk);

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

			blk->flags |= SQFS_BLK_IS_FRAGMENT;

			if (sqfs_data_writer_enqueue(data->proc, blk))
				return -1;
		} else {
			if (sqfs_data_writer_enqueue(data->proc, blk))
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

data_writer_t *data_writer_create(sqfs_super_t *super, sqfs_compressor_t *cmp,
				  sqfs_file_t *file, size_t devblksize,
				  unsigned int num_jobs, size_t max_backlog)
{
	data_writer_t *data = calloc(1, sizeof(*data));

	if (data == NULL) {
		perror("creating data writer");
		return NULL;
	}

	data->proc = sqfs_data_writer_create(super->block_size, cmp,
					     num_jobs, max_backlog,
					     devblksize, file);
	if (data->proc == NULL) {
		perror("creating data block processor");
		free(data);
		return NULL;
	}

	sqfs_data_writer_set_hooks(data->proc, data, &hooks);

	data->cmp = cmp;
	data->super = super;
	return data;
}

void data_writer_destroy(data_writer_t *data)
{
	sqfs_data_writer_destroy(data->proc);
	free(data);
}

int data_writer_write_fragment_table(data_writer_t *data)
{
	if (sqfs_data_writer_write_fragment_table(data->proc, data->super)) {
		fputs("error storing fragment table\n", stderr);
		return -1;
	}
	return 0;
}

int data_writer_sync(data_writer_t *data)
{
	return sqfs_data_writer_finish(data->proc);
}

data_writer_stats_t *data_writer_get_stats(data_writer_t *data)
{
	return &data->stats;
}
