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

struct sqfs_block_writer_t {
	sqfs_object_t *base;
	sqfs_file_t *file;

	size_t num_blocks;
	size_t max_blocks;
	blk_info_t *blocks;
	size_t devblksz;

	sqfs_block_writer_stats_t stats;

	const sqfs_block_hooks_t *hooks;
	void *user_ptr;

	sqfs_u64 data_area_start;

	sqfs_u64 start;
	size_t file_start;
};

static int store_block_location(sqfs_block_writer_t *wr, sqfs_u64 offset,
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

static size_t deduplicate_blocks(sqfs_block_writer_t *wr, size_t count)
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

static int align_file(sqfs_block_writer_t *wr)
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

	if (wr->hooks != NULL && wr->hooks->prepare_padding != NULL)
		wr->hooks->prepare_padding(wr->user_ptr, padding, diff);

	ret = wr->file->write_at(wr->file, size, padding, diff);
	free(padding);
	if (ret)
		return ret;

	return store_block_location(wr, size, 0, 0);
}

static void block_writer_destroy(sqfs_object_t *wr)
{
	free(((sqfs_block_writer_t *)wr)->blocks);
	free(wr);
}

sqfs_block_writer_t *sqfs_block_writer_create(sqfs_file_t *file,
					      size_t devblksz, sqfs_u32 flags)
{
	sqfs_block_writer_t *wr;

	if (flags != 0)
		return NULL;

	wr = calloc(1, sizeof(*wr));
	if (wr == NULL)
		return NULL;

	((sqfs_object_t *)wr)->destroy = block_writer_destroy;
	wr->file = file;
	wr->devblksz = devblksz;
	wr->max_blocks = INIT_BLOCK_COUNT;
	wr->stats.size = sizeof(wr->stats);
	wr->data_area_start = wr->file->get_size(wr->file);

	wr->blocks = alloc_array(sizeof(wr->blocks[0]), wr->max_blocks);
	if (wr->blocks == NULL) {
		free(wr);
		return NULL;
	}

	return wr;
}

int sqfs_block_writer_set_hooks(sqfs_block_writer_t *wr, void *user_ptr,
				const sqfs_block_hooks_t *hooks)
{
	if (hooks->size != sizeof(*hooks))
		return SQFS_ERROR_UNSUPPORTED;

	wr->hooks = hooks;
	wr->user_ptr = user_ptr;
	return 0;
}

int sqfs_block_writer_write(sqfs_block_writer_t *wr, sqfs_u32 size,
			    sqfs_u32 checksum, sqfs_u32 flags,
			    const sqfs_u8 *data, sqfs_u64 *location)
{
	size_t start, count;
	sqfs_u64 offset;
	sqfs_u32 out;
	int err;

	if (wr->hooks != NULL && wr->hooks->pre_block_write != NULL) {
		out = flags;
		flags &= ~SQFS_BLK_USER_SETTABLE_FLAGS;

		wr->hooks->pre_block_write(wr->user_ptr, &out, size,
					   data, wr->file);

		flags |= out & SQFS_BLK_USER_SETTABLE_FLAGS;
	}

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

		wr->stats.bytes_submitted += size;
		wr->stats.blocks_submitted += 1;
		wr->stats.blocks_written = wr->num_blocks;
		wr->stats.bytes_written = offset + size - wr->data_area_start;
	}

	if (wr->hooks != NULL && wr->hooks->post_block_write != NULL) {
		wr->hooks->post_block_write(wr->user_ptr, flags, size, data,
					    wr->file);
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
		} else {
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
		}

		err = wr->file->truncate(wr->file, wr->start);
		if (err)
			return err;

		wr->stats.blocks_written = wr->num_blocks;
		wr->stats.bytes_written = wr->start - wr->data_area_start;
	}

	return 0;
}

const sqfs_block_writer_stats_t
*sqfs_block_writer_get_stats(const sqfs_block_writer_t *wr)
{
	return &wr->stats;
}
