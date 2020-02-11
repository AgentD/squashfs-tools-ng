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

sqfs_data_reader_t *sqfs_data_reader_create(sqfs_file_t *file,
					    size_t block_size,
					    sqfs_compressor_t *cmp)
{
	sqfs_data_reader_t *data = alloc_flex(sizeof(*data), 1, block_size);

	if (data == NULL)
		return NULL;

	data->frag_tbl = sqfs_frag_table_create(0);
	if (data->frag_tbl == NULL) {
		free(data);
		return NULL;
	}

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

void sqfs_data_reader_destroy(sqfs_data_reader_t *data)
{
	sqfs_frag_table_destroy(data->frag_tbl);
	free(data->data_block);
	free(data->frag_block);
	free(data);
}

int sqfs_data_reader_get_block(sqfs_data_reader_t *data,
			       const sqfs_inode_generic_t *inode,
			       size_t index, size_t *size, sqfs_u8 **out)
{
	size_t i, unpacked_size;
	sqfs_u64 off, filesz;

	sqfs_inode_get_file_block_start(inode, &off);
	sqfs_inode_get_file_size(inode, &filesz);

	if (index >= inode->num_file_blocks)
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
	sqfs_u64 filesz;
	int err;

	sqfs_inode_get_file_size(inode, &filesz);
	sqfs_inode_get_frag_location(inode, &frag_idx, &frag_off);
	*size = 0;
	*out = NULL;

	if (inode->num_file_blocks * data->block_size >= filesz)
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
	sqfs_u64 off, filesz;
	char *ptr;
	size_t i;
	int err;

	if (size >= 0x7FFFFFFF)
		size = 0x7FFFFFFE;

	/* work out file location and size */
	sqfs_inode_get_file_size(inode, &filesz);
	sqfs_inode_get_frag_location(inode, &frag_idx, &frag_off);
	sqfs_inode_get_file_block_start(inode, &off);

	/* find location of the first block */
	i = 0;

	while (offset > data->block_size && i < inode->num_file_blocks) {
		off += SQFS_ON_DISK_BLOCK_SIZE(inode->extra[i++]);
		offset -= data->block_size;

		if (filesz >= data->block_size) {
			filesz -= data->block_size;
		} else {
			filesz = 0;
		}
	}

	/* copy data from blocks */
	while (i < inode->num_file_blocks && size > 0 && filesz > 0) {
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

		if (filesz >= data->block_size) {
			filesz -= data->block_size;
		} else {
			filesz = 0;
		}

		++i;
		offset = 0;
		size -= diff;
		total += diff;
		buffer = (char *)buffer + diff;
	}

	/* copy from fragment */
	if (i == inode->num_file_blocks && size > 0 && filesz > 0) {
		err = precache_fragment_block(data, frag_idx);
		if (err)
			return err;

		if (frag_off + filesz > data->block_size)
			return SQFS_ERROR_OUT_OF_BOUNDS;

		if (offset >= filesz)
			return total;

		if (offset + size > filesz)
			size = filesz - offset;

		if (size == 0)
			return total;

		ptr = (char *)data->frag_block + frag_off + offset;
		memcpy(buffer, ptr, size);
		total += size;
	}

	return total;
}
