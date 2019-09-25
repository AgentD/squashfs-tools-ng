/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/data_writer.h"
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

	data->proc = sqfs_data_writer_create(super->block_size, cmp,
					     num_jobs, max_backlog,
					     devblksize, file);
	if (data->proc == NULL) {
		perror("creating data block processor");
		free(data);
		return NULL;
	}

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
