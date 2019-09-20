/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_reader.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "data_reader.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

struct data_reader_t {
	sqfs_fragment_t *frag;
	size_t num_fragments;
	size_t current_frag_index;
	size_t frag_used;

	off_t current_block;

	sqfs_compressor_t *cmp;
	size_t block_size;
	sqfs_file_t *file;

	void *block;
	void *scratch;
	void *frag_block;
};

static ssize_t read_block(data_reader_t *data, off_t offset, uint32_t size,
			  void *dst)
{
	bool compressed = SQFS_IS_BLOCK_COMPRESSED(size);
	void *ptr = compressed ? data->scratch : dst;
	ssize_t ret;

	size = SQFS_ON_DISK_BLOCK_SIZE(size);

	if (size > data->block_size)
		goto fail_bs;

	if (data->file->read_at(data->file, offset, ptr, size)) {
		fputs("error reading data block from input file\n", stderr);
		return -1;
	}

	if (compressed) {
		ret = data->cmp->do_block(data->cmp, data->scratch, size,
					  dst, data->block_size);
		if (ret <= 0) {
			fputs("extracting block failed\n", stderr);
			return -1;
		}
		size = ret;
	}

	return size;
fail_bs:
	fputs("found compressed block larger than block size\n", stderr);
	return -1;
}

static int precache_data_block(data_reader_t *data, off_t location,
			       uint32_t size)
{
	ssize_t ret;

	if (data->current_block == location)
		return 0;

	ret = read_block(data, location, size, data->block);
	if (ret < 0)
		return -1;

	if ((size_t)ret < data->block_size)
		memset((char *)data->block + ret, 0, data->block_size - ret);

	data->current_block = location;
	return 0;
}

static int precache_fragment_block(data_reader_t *data, size_t idx)
{
	ssize_t ret;

	if (idx == data->current_frag_index)
		return 0;

	if (idx >= data->num_fragments) {
		fputs("fragment index out of bounds\n", stderr);
		return -1;
	}

	ret = read_block(data, data->frag[idx].start_offset,
			 data->frag[idx].size, data->frag_block);
	if (ret < 0)
		return -1;

	data->current_frag_index = idx;
	data->frag_used = ret;
	return 0;
}

data_reader_t *data_reader_create(sqfs_file_t *file, sqfs_super_t *super,
				  sqfs_compressor_t *cmp)
{
	data_reader_t *data = alloc_flex(sizeof(*data), super->block_size, 3);
	size_t i, size;
	void *raw_frag;
	int ret;

	if (data == NULL) {
		perror("creating data reader");
		return data;
	}

	data->num_fragments = super->fragment_entry_count;
	data->current_frag_index = super->fragment_entry_count;
	data->block = (char *)data + sizeof(*data);
	data->scratch = (char *)data->block + super->block_size;
	data->frag_block = (char *)data->scratch + super->block_size;
	data->current_block = -1;
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
	free(data->frag);
	free(data);
}

int data_reader_dump(data_reader_t *data, const sqfs_inode_generic_t *inode,
		     int outfd, bool allow_sparse)
{
	uint32_t frag_idx, frag_off;
	uint64_t filesz;
	size_t i, diff;
	off_t off;

	if (inode->base.type == SQFS_INODE_FILE) {
		filesz = inode->data.file.file_size;
		off = inode->data.file.blocks_start;
		frag_idx = inode->data.file.fragment_index;
		frag_off = inode->data.file.fragment_offset;
	} else if (inode->base.type == SQFS_INODE_EXT_FILE) {
		filesz = inode->data.file_ext.file_size;
		off = inode->data.file_ext.blocks_start;
		frag_idx = inode->data.file_ext.fragment_idx;
		frag_off = inode->data.file_ext.fragment_offset;
	} else {
		return -1;
	}

	if (allow_sparse && ftruncate(outfd, filesz))
		goto fail_sparse;

	for (i = 0; i < inode->num_file_blocks; ++i) {
		diff = filesz > data->block_size ? data->block_size : filesz;
		filesz -= diff;

		if (SQFS_IS_SPARSE_BLOCK(inode->block_sizes[i])) {
			if (allow_sparse) {
				if (lseek(outfd, diff, SEEK_CUR) == (off_t)-1)
					goto fail_sparse;
				continue;
			}
			memset(data->block, 0, diff);
		} else {
			if (precache_data_block(data, off,
						inode->block_sizes[i]))
				return -1;
			off += SQFS_ON_DISK_BLOCK_SIZE(inode->block_sizes[i]);
		}

		if (write_data("writing uncompressed block",
			       outfd, data->block, diff)) {
			return -1;
		}
	}

	if (filesz > 0 && frag_off != 0xFFFFFFFF) {
		if (precache_fragment_block(data, frag_idx))
			return -1;

		if (frag_off >= data->frag_used)
			goto fail_range;

		if ((frag_off + filesz - 1) >= data->frag_used)
			goto fail_range;

		if (write_data("writing uncompressed fragment", outfd,
			       (char *)data->frag_block + frag_off,
			       filesz)) {
			return -1;
		}
	}

	return 0;
fail_range:
	fputs("attempted to read past fragment block limits\n", stderr);
	return -1;
fail_sparse:
	perror("creating sparse output file");
	return -1;
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

			memcpy(buffer, (char *)data->block + offset, diff);
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

		if (frag_off >= data->frag_used)
			goto fail_range;

		if (frag_off + filesz > data->frag_used)
			goto fail_range;

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
fail_range:
	fputs("attempted to read past fragment block limits\n", stderr);
	return -1;
}
