/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/error.h"

#include "data_reader.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

struct data_reader_t {
	sqfs_fragment_t *frag;
	sqfs_compressor_t *cmp;
	sqfs_block_t *data_block;
	sqfs_block_t *frag_block;

	uint64_t current_block;

	sqfs_file_t *file;

	uint32_t num_fragments;
	uint32_t current_frag_index;
	uint32_t block_size;

	uint8_t scratch[];
};

static int get_block(data_reader_t *data, uint64_t off, uint32_t size,
		     size_t unpacked_size, sqfs_block_t **out)
{
	sqfs_block_t *blk = alloc_flex(sizeof(*blk), 1, unpacked_size);
	size_t on_disk_size;
	ssize_t ret;
	int err;

	if (blk == NULL)
		return SQFS_ERROR_ALLOC;

	blk->size = unpacked_size;

	if (SQFS_IS_SPARSE_BLOCK(size)) {
		*out = blk;
		return 0;
	}

	on_disk_size = SQFS_ON_DISK_BLOCK_SIZE(size);

	if (on_disk_size > unpacked_size)
		return SQFS_ERROR_OVERFLOW;

	if (SQFS_IS_BLOCK_COMPRESSED(size)) {
		err = data->file->read_at(data->file, off,
					  data->scratch, on_disk_size);
		if (err) {
			free(blk);
			return err;
		}

		ret = data->cmp->do_block(data->cmp, data->scratch,
					  on_disk_size, blk->data, blk->size);
		if (ret <= 0)
			err = ret < 0 ? ret : SQFS_ERROR_OVERFLOW;
	} else {
		err = data->file->read_at(data->file, off,
					  blk->data, on_disk_size);
	}

	if (err) {
		free(blk);
		return err;
	}

	*out = blk;
	return 0;
}

static int precache_data_block(data_reader_t *data, uint64_t location,
			       uint32_t size)
{
	int ret;

	if (data->data_block != NULL && data->current_block == location)
		return 0;

	free(data->data_block);

	ret = get_block(data, location, size, data->block_size,
			&data->data_block);

	if (ret < 0) {
		data->data_block = NULL;
		return -1;
	}

	data->current_block = location;
	return 0;
}

static int precache_fragment_block(data_reader_t *data, size_t idx)
{
	int ret;

	if (data->frag_block != NULL && idx == data->current_frag_index)
		return 0;

	if (idx >= data->num_fragments) {
		fputs("fragment index out of bounds\n", stderr);
		return -1;
	}

	free(data->frag_block);

	ret = get_block(data, data->frag[idx].start_offset,
			data->frag[idx].size, data->block_size,
			&data->frag_block);
	if (ret < 0)
		return -1;

	data->current_frag_index = idx;
	return 0;
}

data_reader_t *data_reader_create(sqfs_file_t *file, sqfs_super_t *super,
				  sqfs_compressor_t *cmp)
{
	data_reader_t *data = alloc_flex(sizeof(*data), 1, super->block_size);
	size_t i, size;
	void *raw_frag;
	int ret;

	if (data == NULL) {
		perror("creating data reader");
		return data;
	}

	data->num_fragments = super->fragment_entry_count;
	data->current_frag_index = super->fragment_entry_count;
	data->file = file;
	data->block_size = super->block_size;
	data->cmp = cmp;

	if (super->fragment_entry_count == 0 ||
	    (super->flags & SQFS_FLAG_NO_FRAGMENTS) != 0) {
		return data;
	}

	if (super->fragment_table_start >= super->bytes_used) {
		fputs("Fragment table start is past end of file\n", stderr);
		free(data);
		return NULL;
	}

	if (SZ_MUL_OV(sizeof(data->frag[0]), data->num_fragments, &size)) {
		fputs("Too many fragments: overflow\n", stderr);
		free(data);
		return NULL;
	}

	ret = sqfs_read_table(file, cmp, size, super->fragment_table_start,
			      super->directory_table_start,
			      super->fragment_table_start, &raw_frag);

	if (ret) {
		free(data);
		return NULL;
	}

	data->frag = raw_frag;

	for (i = 0; i < data->num_fragments; ++i) {
		data->frag[i].size = le32toh(data->frag[i].size);
		data->frag[i].start_offset =
			le64toh(data->frag[i].start_offset);
	}

	return data;
}

void data_reader_destroy(data_reader_t *data)
{
	free(data->data_block);
	free(data->frag_block);
	free(data->frag);
	free(data);
}

int data_reader_get_block(data_reader_t *data,
			  const sqfs_inode_generic_t *inode,
			  size_t index, sqfs_block_t **out)
{
	size_t i, unpacked_size;
	uint64_t off, filesz;

	if (inode->base.type == SQFS_INODE_FILE) {
		off = inode->data.file.blocks_start;
		filesz = inode->data.file.file_size;
	} else if (inode->base.type == SQFS_INODE_EXT_FILE) {
		off = inode->data.file_ext.blocks_start;
		filesz = inode->data.file_ext.file_size;
	} else {
		return SQFS_ERROR_NOT_FILE;
	}

	if (index >= inode->num_file_blocks)
		return SQFS_ERROR_OUT_OF_BOUNDS;

	for (i = 0; i < index; ++i) {
		off += SQFS_ON_DISK_BLOCK_SIZE(inode->block_sizes[i]);
		filesz -= data->block_size;
	}

	unpacked_size = filesz < data->block_size ? filesz : data->block_size;

	return get_block(data, off, inode->block_sizes[index],
			 unpacked_size, out);
}

int data_reader_get_fragment(data_reader_t *data,
			     const sqfs_inode_generic_t *inode,
			     sqfs_block_t **out)
{
	uint32_t frag_idx, frag_off, frag_sz;
	sqfs_block_t *blk;
	uint64_t filesz;

	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		filesz = inode->data.file_ext.file_size;
		frag_idx = inode->data.file_ext.fragment_idx;
		frag_off = inode->data.file_ext.fragment_offset;
	} else if (inode->base.type == SQFS_INODE_FILE) {
		filesz = inode->data.file.file_size;
		frag_idx = inode->data.file.fragment_index;
		frag_off = inode->data.file.fragment_offset;
	} else {
		return -1;
	}

	if (inode->num_file_blocks * data->block_size >= filesz) {
		*out = NULL;
		return 0;
	}

	frag_sz = filesz % data->block_size;

	if (precache_fragment_block(data, frag_idx))
		return -1;

	if (frag_off + frag_sz > data->block_size)
		return -1;

	blk = alloc_flex(sizeof(*blk), 1, frag_sz);
	if (blk == NULL)
		return -1;

	blk->size = frag_sz;
	memcpy(blk->data, (char *)data->frag_block->data + frag_off, frag_sz);

	*out = blk;
	return 0;
}

ssize_t data_reader_read(data_reader_t *data,
			 const sqfs_inode_generic_t *inode,
			 uint64_t offset, void *buffer, size_t size)
{
	uint32_t frag_idx, frag_off;
	size_t i, diff, total = 0;
	uint64_t off, filesz;
	char *ptr;

	/* work out file location and size */
	if (inode->base.type == SQFS_INODE_EXT_FILE) {
		off = inode->data.file_ext.blocks_start;
		filesz = inode->data.file_ext.file_size;
		frag_idx = inode->data.file_ext.fragment_idx;
		frag_off = inode->data.file_ext.fragment_offset;
	} else {
		off = inode->data.file.blocks_start;
		filesz = inode->data.file.file_size;
		frag_idx = inode->data.file.fragment_index;
		frag_off = inode->data.file.fragment_offset;
	}

	/* find location of the first block */
	i = 0;

	while (offset > data->block_size && i < inode->num_file_blocks) {
		off += SQFS_ON_DISK_BLOCK_SIZE(inode->block_sizes[i++]);
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

		if (SQFS_IS_SPARSE_BLOCK(inode->block_sizes[i])) {
			memset(buffer, 0, diff);
		} else {
			if (precache_data_block(data, off,
						inode->block_sizes[i])) {
				return -1;
			}

			memcpy(buffer, (char *)data->data_block->data + offset,
			       diff);
			off += SQFS_ON_DISK_BLOCK_SIZE(inode->block_sizes[i]);
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
		if (precache_fragment_block(data, frag_idx))
			return -1;

		if (frag_off + filesz > data->block_size)
			goto fail_range;

		if (offset >= filesz)
			return total;

		if (offset + size > filesz)
			size = filesz - offset;

		if (size == 0)
			return total;

		ptr = (char *)data->frag_block->data + frag_off + offset;
		memcpy(buffer, ptr, size);
		total += size;
	}

	return total;
fail_range:
	fputs("attempted to read past fragment block limits\n", stderr);
	return -1;
}
