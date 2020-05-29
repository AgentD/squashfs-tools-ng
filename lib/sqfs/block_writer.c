/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * block_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/block_writer.h"
#include "sqfs/error.h"
#include "sqfs/block.h"
#include "sqfs/io.h"
#include "util.h"

#include <stdlib.h>

#define MK_BLK_HASH(chksum, size) \
	(((sqfs_u64)(size) << 32) | (sqfs_u64)(chksum))

#define INIT_BLOCK_COUNT (128)

typedef struct {
	sqfs_u64 offset;
	sqfs_u64 hash;
} blk_info_t;

typedef struct {
	sqfs_block_writer_t base;
	sqfs_file_t *file;

	size_t num_blocks;
	size_t max_blocks;
	blk_info_t *blocks;
	size_t devblksz;

	sqfs_u64 blocks_written;
	sqfs_u64 data_area_start;

	sqfs_u64 start;
	size_t file_start;
} block_writer_default_t;

static int store_block_location(block_writer_default_t *wr, sqfs_u64 offset,
				sqfs_u32 size, sqfs_u32 chksum)
{
	size_t new_sz;
	void *new;

	if (wr->num_blocks == wr->max_blocks) {
		new_sz = wr->max_blocks * 2;
		new = realloc(wr->blocks, sizeof(wr->blocks[0]) * new_sz);

		if (new == NULL)
			return SQFS_ERROR_ALLOC;

		wr->blocks = new;
		wr->max_blocks = new_sz;
	}

	wr->blocks[wr->num_blocks].offset = offset;
	wr->blocks[wr->num_blocks].hash = MK_BLK_HASH(chksum, size);
	wr->num_blocks += 1;
	return 0;
}

static size_t deduplicate_blocks(block_writer_default_t *wr, size_t count)
{
	size_t i, j;

	for (i = 0; i < wr->file_start; ++i) {
		for (j = 0; j < count; ++j) {
			if (wr->blocks[i + j].hash == 0)
				break;

			if (wr->blocks[i + j].hash !=
			    wr->blocks[wr->file_start + j].hash)
				break;
		}

		if (j == count)
			break;
	}

	return i;
}

static int align_file(block_writer_default_t *wr)
{
	void *padding;
	sqfs_u64 size;
	size_t diff;
	int ret;

	size = wr->file->get_size(wr->file);
	diff = size % wr->devblksz;
	if (diff == 0)
		return 0;

	padding = calloc(1, diff);
	if (padding == 0)
		return SQFS_ERROR_ALLOC;

	ret = wr->file->write_at(wr->file, size, padding, diff);
	free(padding);
	if (ret)
		return ret;

	return store_block_location(wr, size, 0, 0);
}

static void block_writer_destroy(sqfs_object_t *wr)
{
	free(((block_writer_default_t *)wr)->blocks);
	free(wr);
}

static int write_data_block(sqfs_block_writer_t *base, void *user,
			    sqfs_u32 size, sqfs_u32 checksum, sqfs_u32 flags,
			    const sqfs_u8 *data, sqfs_u64 *location)
{
	block_writer_default_t *wr = (block_writer_default_t *)base;
	size_t start, count;
	sqfs_u64 offset;
	sqfs_u32 out;
	int err;
	(void)user;

	if (flags & SQFS_BLK_FIRST_BLOCK) {
		wr->start = wr->file->get_size(wr->file);
		wr->file_start = wr->num_blocks;

		if (flags & SQFS_BLK_ALIGN) {
			err = align_file(wr);
			if (err)
				return err;
		}
	}

	offset = wr->file->get_size(wr->file);
	*location = offset;

	if (size != 0 && !(flags & SQFS_BLK_IS_SPARSE)) {
		out = size;
		if (!(flags & SQFS_BLK_IS_COMPRESSED))
			out |= 1 << 24;

		err = store_block_location(wr, offset, out, checksum);
		if (err)
			return err;

		err = wr->file->write_at(wr->file, offset, data, size);
		if (err)
			return err;

		wr->blocks_written = wr->num_blocks;
	}

	if (flags & SQFS_BLK_LAST_BLOCK) {
		if (flags & SQFS_BLK_ALIGN) {
			err = align_file(wr);
			if (err)
				return err;
		}

		count = wr->num_blocks - wr->file_start;

		if (count == 0) {
			*location = 0;
		} else if (!(flags & SQFS_BLK_DONT_DEDUPLICATE)) {
			start = deduplicate_blocks(wr, count);
			offset = wr->blocks[start].offset;

			*location = offset;
			if (start >= wr->file_start)
				return 0;

			offset = start + count;
			if (offset >= wr->file_start) {
				count = wr->num_blocks - offset;
				wr->num_blocks = offset;
			} else {
				wr->num_blocks = wr->file_start;
			}

			err = wr->file->truncate(wr->file, wr->start);
			if (err)
				return err;
		}

		wr->blocks_written = wr->num_blocks;
	}

	return 0;
}

static sqfs_u64 get_block_count(const sqfs_block_writer_t *wr)
{
	return ((const block_writer_default_t *)wr)->blocks_written;
}

sqfs_block_writer_t *sqfs_block_writer_create(sqfs_file_t *file,
					      size_t devblksz, sqfs_u32 flags)
{
	block_writer_default_t *wr;

	if (flags != 0)
		return NULL;

	wr = calloc(1, sizeof(*wr));
	if (wr == NULL)
		return NULL;

	((sqfs_block_writer_t *)wr)->write_data_block = write_data_block;
	((sqfs_block_writer_t *)wr)->get_block_count = get_block_count;
	((sqfs_object_t *)wr)->destroy = block_writer_destroy;
	wr->file = file;
	wr->devblksz = devblksz;
	wr->max_blocks = INIT_BLOCK_COUNT;
	wr->data_area_start = wr->file->get_size(wr->file);

	wr->blocks = alloc_array(sizeof(wr->blocks[0]), wr->max_blocks);
	if (wr->blocks == NULL) {
		free(wr);
		return NULL;
	}

	return (sqfs_block_writer_t *)wr;
}
