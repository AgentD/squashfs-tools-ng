/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "data_reader.h"
#include "frag_reader.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

struct data_reader_t {
	compressor_t *cmp;
	size_t block_size;
	frag_reader_t *frag;
	int sqfsfd;

	void *buffer;
	void *scratch;
};

data_reader_t *data_reader_create(int fd, sqfs_super_t *super,
				  compressor_t *cmp)
{
	data_reader_t *data = calloc(1, sizeof(*data) + 2 * super->block_size);

	if (data == NULL) {
		perror("creating data reader");
		return data;
	}

	if (super->fragment_entry_count > 0 &&
	    !(super->flags & SQFS_FLAG_NO_FRAGMENTS)) {
		if (super->fragment_table_start >= super->bytes_used) {
			fputs("Fragment table start is past end of file\n",
			      stderr);
			free(data);
			return NULL;
		}

		data->frag = frag_reader_create(super, fd, cmp);
		if (data->frag == NULL) {
			free(data);
			return NULL;
		}
	}

	data->buffer = (char *)data + sizeof(*data);
	data->scratch = (char *)data->buffer + super->block_size;
	data->sqfsfd = fd;
	data->block_size = super->block_size;
	data->cmp = cmp;
	return data;
}

void data_reader_destroy(data_reader_t *data)
{
	if (data->frag != NULL)
		frag_reader_destroy(data->frag);
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
		bs = fi->blocksizes[i];

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

int data_reader_dump_file(data_reader_t *data, file_info_t *fi, int outfd,
			  bool allow_sparse)
{
	size_t fragsz = fi->size % data->block_size;
	size_t count = fi->size / data->block_size;

	if (fragsz != 0 && (fi->fragment == 0xFFFFFFFF ||
			    fi->fragment_offset == 0xFFFFFFFF)) {
		fragsz = 0;
		++count;
	}

	if (dump_blocks(data, fi, outfd, allow_sparse, count))
		return -1;

	if (fragsz > 0) {
		if (data->frag == NULL)
			goto fail_frag;

		if (frag_reader_read(data->frag, fi->fragment,
				     fi->fragment_offset, data->buffer,
				     fragsz)) {
			return -1;
		}

		if (write_data("writing uncompressed fragment",
			       outfd, data->buffer, fragsz)) {
			return -1;
		}
	}

	return 0;
fail_frag:
	fputs("file not a multiple of block size but no fragments available\n",
	      stderr);
	return -1;
}

const frag_reader_t *data_reader_get_fragment_reader(const data_reader_t *data)
{
	return data->frag;
}
