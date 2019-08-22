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

	compressor_t *cmp;
	size_t block_size;
	int sqfsfd;

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

	if (read_data_at("reading block", offset, data->sqfsfd, ptr, size))
		return -1;

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

data_reader_t *data_reader_create(int fd, sqfs_super_t *super,
				  compressor_t *cmp)
{
	data_reader_t *data = alloc_flex(sizeof(*data), super->block_size, 3);
	size_t i, size;

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
	data->sqfsfd = fd;
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

	data->frag = sqfs_read_table(fd, cmp, size,
				     super->fragment_table_start);
	if (data->frag == NULL) {
		free(data);
		return NULL;
	}

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

int data_reader_dump_file(data_reader_t *data, file_info_t *fi, int outfd,
			  bool allow_sparse)
{
	uint64_t filesz = fi->size;
	size_t fragsz = fi->size % data->block_size;
	size_t count = fi->size / data->block_size;
	off_t off = fi->startblock;
	size_t i, diff;

	if (fragsz != 0 && !(fi->flags & FILE_FLAG_HAS_FRAGMENT)) {
		fragsz = 0;
		++count;
	}

	if (allow_sparse && ftruncate(outfd, filesz))
		goto fail_sparse;

	for (i = 0; i < count; ++i) {
		diff = filesz > data->block_size ? data->block_size : filesz;
		filesz -= diff;

		if (SQFS_IS_SPARSE_BLOCK(fi->blocks[i].size)) {
			if (allow_sparse) {
				if (lseek(outfd, diff, SEEK_CUR) == (off_t)-1)
					goto fail_sparse;
				continue;
			}
			memset(data->block, 0, diff);
		} else {
			if (precache_data_block(data, off, fi->blocks[i].size))
				return -1;
			off += SQFS_ON_DISK_BLOCK_SIZE(fi->blocks[i].size);
		}

		if (write_data("writing uncompressed block",
			       outfd, data->block, diff)) {
			return -1;
		}
	}

	if (fragsz > 0) {
		if (precache_fragment_block(data, fi->fragment))
			return -1;

		if (fi->fragment_offset >= data->frag_used)
			goto fail_range;

		if ((fi->fragment_offset + fragsz - 1) >= data->frag_used)
			goto fail_range;

		if (write_data("writing uncompressed fragment", outfd,
			       (char *)data->frag_block + fi->fragment_offset,
			       fragsz)) {
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

ssize_t data_reader_read(data_reader_t *data, file_info_t *fi,
			 uint64_t offset, void *buffer, size_t size)
{
	size_t i, diff, fragsz, count, total = 0;
	off_t off;
	char *ptr;

	/* work out block count and fragment size */
	fragsz = fi->size % data->block_size;
	count = fi->size / data->block_size;

	if (fragsz != 0 && !(fi->flags & FILE_FLAG_HAS_FRAGMENT)) {
		fragsz = 0;
		++count;
	}

	/* work out block index and on-disk location */
	off = fi->startblock;
	i = 0;

	while (offset > data->block_size && i < count) {
		off += SQFS_ON_DISK_BLOCK_SIZE(fi->blocks[i++].size);
		offset -= data->block_size;
	}

	/* copy data from blocks */
	while (i < count && size > 0) {
		diff = data->block_size - offset;
		if (size < diff)
			size = diff;

		if (SQFS_IS_SPARSE_BLOCK(fi->blocks[i].size)) {
			memset(buffer, 0, diff);
		} else {
			if (precache_data_block(data, off, fi->blocks[i].size))
				return -1;

			memcpy(buffer, (char *)data->block + offset, diff);
			off += SQFS_ON_DISK_BLOCK_SIZE(fi->blocks[i].size);
		}

		++i;
		offset = 0;
		size -= diff;
		total += diff;
		buffer = (char *)buffer + diff;
	}

	/* copy from fragment */
	if (i == count && size > 0 && fragsz > 0) {
		if (precache_fragment_block(data, fi->fragment))
			return -1;

		if (fi->fragment_offset >= data->frag_used)
			goto fail_range;

		if ((fi->fragment_offset + fragsz - 1) >= data->frag_used)
			goto fail_range;

		ptr = (char *)data->frag_block + fi->fragment_offset;
		ptr += offset;

		if (offset >= fragsz) {
			offset = 0;
			size = 0;
		}

		if (offset + size > fragsz)
			size = fragsz - offset;

		if (size > 0) {
			memcpy(buffer, ptr + offset, size);
			total += size;
		}
	}
	return total;
fail_range:
	fputs("attempted to read past fragment block limits\n", stderr);
	return -1;
}
