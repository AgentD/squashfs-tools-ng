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

	compressor_t *cmp;
	size_t block_size;
	int sqfsfd;

	void *buffer;
	void *scratch;
	void *frag_block;
};

data_reader_t *data_reader_create(int fd, sqfs_super_t *super,
				  compressor_t *cmp)
{
	data_reader_t *data = calloc(1, sizeof(*data) + 3 * super->block_size);
	size_t i, size;

	if (data == NULL) {
		perror("creating data reader");
		return data;
	}

	data->num_fragments = super->fragment_entry_count;
	data->current_frag_index = super->fragment_entry_count;
	data->buffer = (char *)data + sizeof(*data);
	data->scratch = (char *)data->buffer + super->block_size;
	data->frag_block = (char *)data->scratch + super->block_size;
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

	size = sizeof(data->frag[0]) * data->num_fragments;

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

static int dump_blocks(data_reader_t *data, file_info_t *fi, int outfd,
		       bool allow_sparse, size_t count)
{
	off_t sqfs_location = fi->startblock;
	size_t i, unpackedsz;
	uint64_t filesz = 0;
	bool compressed;
	uint32_t bs;
	ssize_t ret;
	void *ptr;

	for (i = 0; i < count; ++i) {
		bs = fi->blocks[i].size;

		compressed = (bs & (1 << 24)) == 0;
		bs &= (1 << 24) - 1;

		if (bs > data->block_size)
			goto fail_bs;

		if ((fi->size - filesz) < (uint64_t)data->block_size) {
			unpackedsz = fi->size - filesz;
		} else {
			unpackedsz = data->block_size;
		}

		filesz += unpackedsz;

		if (bs == 0 && allow_sparse) {
			if (ftruncate(outfd, filesz))
				goto fail_sparse;
			if (lseek(outfd, 0, SEEK_END) == (off_t)-1)
				goto fail_sparse;
			continue;
		}

		if (bs == 0) {
			memset(data->buffer, 0, unpackedsz);
			compressed = false;
		} else {
			if (read_data_at("reading data block", sqfs_location,
					 data->sqfsfd, data->buffer, bs)) {
				return -1;
			}
			sqfs_location += bs;
		}

		if (compressed) {
			ret = data->cmp->do_block(data->cmp, data->buffer, bs,
						  data->scratch,
						  data->block_size);
			if (ret <= 0)
				return -1;

			ptr = data->scratch;
		} else {
			ptr = data->buffer;
		}

		if (write_data("writing uncompressed block",
			       outfd, ptr, unpackedsz)) {
			return -1;
		}
	}
	return 0;
fail_sparse:
	perror("creating sparse output file");
	return -1;
fail_bs:
	fputs("found compressed block larger than block size\n", stderr);
	return -1;
}

static int precache_fragment_block(data_reader_t *data, size_t idx)
{
	bool compressed;
	size_t size;
	ssize_t ret;

	if (idx == data->current_frag_index)
		return 0;

	if (idx >= data->num_fragments) {
		fputs("fragment index out of bounds\n", stderr);
		return -1;
	}

	compressed = (data->frag[idx].size & (1 << 24)) == 0;
	size = data->frag[idx].size & ((1 << 24) - 1);

	if (size > data->block_size) {
		fputs("found fragment block larger than block size\n", stderr);
		return -1;
	}

	if (read_data_at("reading fragments", data->frag[idx].start_offset,
			 data->sqfsfd, data->buffer, size)) {
		return -1;
	}

	if (compressed) {
		ret = data->cmp->do_block(data->cmp, data->buffer, size,
					  data->frag_block, data->block_size);

		if (ret <= 0) {
			fputs("extracting fragment block failed\n", stderr);
			return -1;
		}

		size = ret;
	} else {
		memcpy(data->frag_block, data->buffer, size);
	}

	data->current_frag_index = idx;
	data->frag_used = size;
	return 0;
}

int data_reader_dump_file(data_reader_t *data, file_info_t *fi, int outfd,
			  bool allow_sparse)
{
	size_t fragsz = fi->size % data->block_size;
	size_t count = fi->size / data->block_size;

	if (fragsz != 0 && !(fi->flags & FILE_FLAG_HAS_FRAGMENT)) {
		fragsz = 0;
		++count;
	}

	if (dump_blocks(data, fi, outfd, allow_sparse, count))
		return -1;

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
}
