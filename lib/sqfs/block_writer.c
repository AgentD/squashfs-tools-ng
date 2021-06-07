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
#include <string.h>

#define MK_BLK_HASH(chksum, size) \
	(((sqfs_u64)(size) << 32) | (sqfs_u64)(chksum))

#define SIZE_FROM_HASH(hash) ((hash >> 32) & ((1 << 24) - 1))

#define INIT_BLOCK_COUNT (128)
#define SCRATCH_SIZE (8192)

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

	sqfs_u32 flags;

	sqfs_u8 scratch[];
} block_writer_default_t;

static int store_block_location(block_writer_default_t *wr, sqfs_u64 offset,
				sqfs_u32 size, sqfs_u32 chksum)
{
	blk_info_t *new;
	size_t new_sz;

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

static int compare_blocks(block_writer_default_t *wr, sqfs_u64 loc_a,
			  sqfs_u64 loc_b, size_t size)
{
	sqfs_u8 *ptr_a = wr->scratch, *ptr_b = ptr_a + SCRATCH_SIZE / 2;
	size_t diff;
	int ret;

	while (size > 0) {
		diff = SCRATCH_SIZE / 2;
		diff = diff > size ? size : diff;

		ret = wr->file->read_at(wr->file, loc_a, ptr_a, diff);
		if (ret != 0)
			return ret;

		ret = wr->file->read_at(wr->file, loc_b, ptr_b, diff);
		if (ret != 0)
			return ret;

		if (memcmp(ptr_a, ptr_b, diff) != 0)
			return 1;

		size -= diff;
		loc_a += diff;
		loc_b += diff;
	}

	return 0;
}

static int deduplicate_blocks(block_writer_default_t *wr, size_t count,
			      size_t *out)
{
	sqfs_u64 loc_a, loc_b;
	size_t i, j, sz;
	int ret;

	for (i = 0; i < wr->file_start; ++i) {
		for (j = 0; j < count; ++j) {
			if (wr->blocks[i + j].hash == 0)
				break;

			if (wr->blocks[i + j].hash !=
			    wr->blocks[wr->file_start + j].hash)
				break;
		}

		if (j != count)
			continue;

		if (wr->flags & SQFS_BLOCK_WRITER_HASH_COMPARE_ONLY)
			break;

		for (j = 0; j < count; ++j) {
			sz = SIZE_FROM_HASH(wr->blocks[i + j].hash);

			loc_a = wr->blocks[i + j].offset;
			loc_b = wr->blocks[wr->file_start + j].offset;

			ret = compare_blocks(wr, loc_a, loc_b, sz);
			if (ret < 0)
				return ret;
			if (ret > 0)
				break;
		}

		if (j == count)
			break;
	}

	*out = i;
	return 0;
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

	if (flags & (SQFS_BLK_FIRST_BLOCK | SQFS_BLK_FRAGMENT_BLOCK)) {
		if (flags & SQFS_BLK_ALIGN) {
			err = align_file(wr);
			if (err)
				return err;
		}

		if (flags & SQFS_BLK_FIRST_BLOCK) {
			wr->start = wr->file->get_size(wr->file);
			wr->file_start = wr->num_blocks;
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

	if (flags & SQFS_BLK_ALIGN) {
		if (flags & (SQFS_BLK_LAST_BLOCK | SQFS_BLK_FRAGMENT_BLOCK)) {
			err = align_file(wr);
			if (err)
				return err;
		}
	}

	if (flags & SQFS_BLK_LAST_BLOCK) {
		count = wr->num_blocks - wr->file_start;

		if (count == 0) {
			*location = 0;
		} else if (!(flags & SQFS_BLK_DONT_DEDUPLICATE)) {
			err = deduplicate_blocks(wr, count, &start);
			if (err)
				return err;

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

	if (flags & ~SQFS_BLOCK_WRITER_ALL_FLAGS)
		return NULL;

	if (flags & SQFS_BLOCK_WRITER_HASH_COMPARE_ONLY) {
		wr = calloc(1, sizeof(*wr));
	} else {
		wr = alloc_flex(sizeof(*wr), 1, SCRATCH_SIZE);
	}

	if (wr == NULL)
		return NULL;

	((sqfs_block_writer_t *)wr)->write_data_block = write_data_block;
	((sqfs_block_writer_t *)wr)->get_block_count = get_block_count;
	((sqfs_object_t *)wr)->destroy = block_writer_destroy;
	wr->flags = flags;
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
