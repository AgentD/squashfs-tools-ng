/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

static int add_sentinel_block(sqfs_data_writer_t *data,
			      sqfs_inode_generic_t *inode,
			      uint32_t flags)
{
	sqfs_block_t *blk = calloc(1, sizeof(*blk));

	if (blk == NULL) {
		perror("creating sentinel block");
		return -1;
	}

	blk->inode = inode;
	blk->flags = flags;

	return sqfs_data_writer_enqueue(data, blk);
}

int write_data_from_file(sqfs_data_writer_t *data, sqfs_inode_generic_t *inode,
			 sqfs_file_t *file, size_t block_size, int flags)
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
		if (filesz - offset > (uint64_t)block_size) {
			diff = block_size;
		} else {
			diff = filesz - offset;
		}

		ret = sqfs_file_create_block(file, offset, diff, inode,
					     blk_flags, &blk);

		if (ret)
			return -1;

		blk->index = i++;

		if (is_zero_block(blk->data, blk->size)) {
			sqfs_inode_make_extended(inode);
			inode->data.file_ext.sparse += blk->size;
			inode->num_file_blocks += 1;

			inode->block_sizes[blk->index] = 0;
			free(blk);
			continue;
		}

		if (diff < block_size && !(flags & DW_DONT_FRAGMENT)) {
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

			if (sqfs_data_writer_enqueue(data, blk))
				return -1;
		} else {
			if (sqfs_data_writer_enqueue(data, blk))
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
	return 0;
}
