/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * data_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/data_reader.h"
#include "sqfs/compressor.h"
#include "sqfs/frag_table.h"
#include "sqfs/block.h"
#include "sqfs/error.h"
#include "sqfs/table.h"
#include "sqfs/inode.h"
#include "sqfs/io.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

struct sqfs_data_reader_t {
	sqfs_object_t obj;

	sqfs_frag_table_t *frag_tbl;
	sqfs_compressor_t *cmp;
	sqfs_file_t *file;

	sqfs_u8 *data_block;
	size_t data_blk_size;
	sqfs_u64 current_block;

	sqfs_u8 *frag_block;
	size_t frag_blk_size;
	sqfs_u32 current_frag_index;
	sqfs_u32 block_size;

	sqfs_u8 scratch[];
};

static int get_block(sqfs_data_reader_t *data, sqfs_u64 off, sqfs_u32 size,
		     sqfs_u32 max_size, size_t *out_sz, sqfs_u8 **out)
{
	sqfs_u32 on_disk_size;
	sqfs_s32 ret;
	int err;

	*out = alloc_array(1, max_size);
	*out_sz = max_size;

	if (*out == NULL) {
		err = SQFS_ERROR_ALLOC;
		goto fail;
	}

	if (SQFS_IS_SPARSE_BLOCK(size))
		return 0;

	on_disk_size = SQFS_ON_DISK_BLOCK_SIZE(size);

	if (on_disk_size > max_size) {
		err = SQFS_ERROR_OVERFLOW;
		goto fail;
	}

	if (SQFS_IS_BLOCK_COMPRESSED(size)) {
		err = data->file->read_at(data->file, off,
					  data->scratch, on_disk_size);
		if (err)
			goto fail;

		ret = data->cmp->do_block(data->cmp, data->scratch,
					  on_disk_size, *out, max_size);
		if (ret <= 0) {
			err = ret < 0 ? ret : SQFS_ERROR_OVERFLOW;
			goto fail;
		}

		*out_sz = ret;
	} else {
		err = data->file->read_at(data->file, off,
					  *out, on_disk_size);
		if (err)
			goto fail;

		*out_sz = on_disk_size;
	}

	return 0;
fail:
	free(*out);
	*out = NULL;
	*out_sz = 0;
	return err;
}

static int precache_data_block(sqfs_data_reader_t *data, sqfs_u64 location,
			       sqfs_u32 size)
{
	if (data->data_block != NULL && data->current_block == location)
		return 0;

	free(data->data_block);
	data->current_block = location;

	return get_block(data, location, size, data->block_size,
			 &data->data_blk_size, &data->data_block);
}

static int precache_fragment_block(sqfs_data_reader_t *data, size_t idx)
{
	sqfs_fragment_t ent;
	int ret;

	if (data->frag_block != NULL && idx == data->current_frag_index)
		return 0;

	ret = sqfs_frag_table_lookup(data->frag_tbl, idx, &ent);
	if (ret != 0)
		return ret;

	free(data->frag_block);
	data->current_frag_index = idx;

	return get_block(data, ent.start_offset, ent.size, data->block_size,
			 &data->frag_blk_size, &data->frag_block);
}

static void data_reader_destroy(sqfs_object_t *obj)
{
	sqfs_data_reader_t *data = (sqfs_data_reader_t *)obj;

	sqfs_destroy(data->frag_tbl);
	free(data->data_block);
	free(data->frag_block);
	free(data);
}

static sqfs_object_t *data_reader_copy(const sqfs_object_t *obj)
{
	const sqfs_data_reader_t *data = (const sqfs_data_reader_t *)obj;
	sqfs_data_reader_t *copy;

	copy = alloc_flex(sizeof(*data), 1, data->block_size);
	if (copy == NULL)
		return NULL;

	memcpy(copy, data, sizeof(*data) + data->block_size);

	copy->frag_tbl = sqfs_copy(data->frag_tbl);
	if (copy->frag_tbl == NULL)
		goto fail_ftbl;

	if (data->data_block != NULL) {
		copy->data_block = malloc(data->data_blk_size);
		if (copy->data_block == NULL)
			goto fail_dblk;

		memcpy(copy->data_block, data->data_block,
		       data->data_blk_size);
	}

	if (copy->frag_block != NULL) {
		copy->frag_block = malloc(copy->frag_blk_size);
		if (copy->frag_block == NULL)
			goto fail_fblk;

		memcpy(copy->frag_block, data->frag_block,
		       data->frag_blk_size);
	}

	/* XXX: file and cmp aren't deep-copied becaues data
	        doesn't own them either. */
	return (sqfs_object_t *)copy;
fail_fblk:
	free(copy->data_block);
fail_dblk:
	sqfs_destroy(copy->frag_tbl);
fail_ftbl:
	free(copy);
	return NULL;
}

sqfs_data_reader_t *sqfs_data_reader_create(sqfs_file_t *file,
					    size_t block_size,
					    sqfs_compressor_t *cmp,
					    sqfs_u32 flags)
{
	sqfs_data_reader_t *data;

	if (flags != 0)
		return NULL;

	data = alloc_flex(sizeof(*data), 1, block_size);
	if (data == NULL)
		return NULL;

	data->frag_tbl = sqfs_frag_table_create(0);
	if (data->frag_tbl == NULL) {
		free(data);
		return NULL;
	}

	((sqfs_object_t *)data)->destroy = data_reader_destroy;
	((sqfs_object_t *)data)->copy = data_reader_copy;
	data->file = file;
	data->block_size = block_size;
	data->cmp = cmp;
	return data;
}

int sqfs_data_reader_load_fragment_table(sqfs_data_reader_t *data,
					 const sqfs_super_t *super)
{
	int ret;

	free(data->frag_block);
	data->frag_block = NULL;
	data->current_frag_index = 0;

	ret = sqfs_frag_table_read(data->frag_tbl, data->file,
				   super, data->cmp);
	if (ret != 0)
		return ret;

	data->current_frag_index = sqfs_frag_table_get_size(data->frag_tbl);
	return 0;
}

int sqfs_data_reader_get_block(sqfs_data_reader_t *data,
			       const sqfs_inode_generic_t *inode,
			       size_t index, size_t *size, sqfs_u8 **out)
{
	size_t i, unpacked_size;
	sqfs_u64 off, filesz;

	sqfs_inode_get_file_block_start(inode, &off);
	sqfs_inode_get_file_size(inode, &filesz);

	if (index >= sqfs_inode_get_file_block_count(inode))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	for (i = 0; i < index; ++i) {
		off += SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]);
		filesz -= data->block_size;
	}

	unpacked_size = filesz < data->block_size ? filesz : data->block_size;

	return get_block(data, off, inode->extra[index],
			 unpacked_size, size, out);
}

int sqfs_data_reader_get_fragment(sqfs_data_reader_t *data,
				  const sqfs_inode_generic_t *inode,
				  size_t *size, sqfs_u8 **out)
{
	sqfs_u32 frag_idx, frag_off, frag_sz;
	size_t block_count;
	sqfs_u64 filesz;
	int err;

	sqfs_inode_get_file_size(inode, &filesz);
	sqfs_inode_get_frag_location(inode, &frag_idx, &frag_off);
	*size = 0;
	*out = NULL;

	block_count = sqfs_inode_get_file_block_count(inode);

	if (block_count > (UINT64_MAX / data->block_size))
		return SQFS_ERROR_OVERFLOW;

	if ((sqfs_u64)block_count * data->block_size >= filesz)
		return 0;

	frag_sz = filesz % data->block_size;

	err = precache_fragment_block(data, frag_idx);
	if (err)
		return err;

	if (frag_off + frag_sz > data->block_size)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	*out = alloc_array(1, frag_sz);
	if (*out == NULL)
		return SQFS_ERROR_ALLOC;

	*size = frag_sz;
	memcpy(*out, (char *)data->frag_block + frag_off, frag_sz);
	return 0;
}

sqfs_s32 sqfs_data_reader_read(sqfs_data_reader_t *data,
			       const sqfs_inode_generic_t *inode,
			       sqfs_u64 offset, void *buffer, sqfs_u32 size)
{
	sqfs_u32 frag_idx, frag_off, diff, total = 0;
	size_t i, block_count;
	sqfs_u64 off, filesz;
	char *ptr;
	int err;

	if (size >= 0x7FFFFFFF)
		size = 0x7FFFFFFE;

	/* work out file location and size */
	sqfs_inode_get_file_size(inode, &filesz);
	sqfs_inode_get_frag_location(inode, &frag_idx, &frag_off);
	sqfs_inode_get_file_block_start(inode, &off);
	block_count = sqfs_inode_get_file_block_count(inode);

	if (offset >= filesz)
		return 0;

	if ((filesz - offset) < (sqfs_u64)size)
		size = filesz - offset;

	if (size == 0)
		return 0;

	/* find location of the first block */
	for (i = 0; offset > data->block_size && i < block_count; ++i) {
		off += SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]);
		offset -= data->block_size;
	}

	/* copy data from blocks */
	while (i < block_count && size > 0) {
		diff = data->block_size - offset;
		if (size < diff)
			diff = size;

		if (SQFS_IS_SPARSE_BLOCK(inode->extra[i])) {
			memset(buffer, 0, diff);
		} else {
			err = precache_data_block(data, off, inode->extra[i]);
			if (err)
				return err;

			memcpy(buffer, (char *)data->data_block + offset, diff);
			off += SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i]);
		}

		++i;
		offset = 0;
		size -= diff;
		total += diff;
		buffer = (char *)buffer + diff;
	}

	/* copy from fragment */
	if (size > 0) {
		err = precache_fragment_block(data, frag_idx);
		if (err)
			return err;

		if ((frag_off + offset) >= data->frag_blk_size)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		if ((data->frag_blk_size - (frag_off + offset)) < size)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		ptr = (char *)data->frag_block + frag_off + offset;
		memcpy(buffer, ptr, size);
		total += size;
	}

	return total;
}
