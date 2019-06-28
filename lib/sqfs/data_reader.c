/* SPDX-License-Identifier: GPL-3.0-or-later */
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

int data_reader_dump_file(data_reader_t *data, file_info_t *fi, int outfd)
{
	size_t i, count, fragsz;
	bool compressed;
	uint32_t bs;
	ssize_t ret;
	void *ptr;

	count = fi->size / data->block_size;

	if (count > 0) {
		if (lseek(data->sqfsfd, fi->startblock, SEEK_SET) == (off_t)-1)
			goto fail_seek;

		for (i = 0; i < count; ++i) {
			bs = fi->blocksizes[i];

			compressed = (bs & (1 << 24)) == 0;
			bs &= (1 << 24) - 1;

			if (bs > data->block_size)
				goto fail_bs;

			if (bs == 0) {
				bs = data->block_size;
				memset(data->buffer, 0, bs);
				compressed = false;
			} else {
				ret = read_retry(data->sqfsfd, data->buffer, bs);
				if (ret < 0)
					goto fail_rd;

				if ((size_t)ret < bs)
					goto fail_trunc;
			}

			if (compressed) {
				ret = data->cmp->do_block(data->cmp,
							  data->buffer, bs,
							  data->scratch,
							  data->block_size);
				if (ret <= 0)
					return -1;

				bs = ret;
				ptr = data->scratch;
			} else {
				ptr = data->buffer;
			}

			ret = write_retry(outfd, ptr, bs);
			if (ret < 0)
				goto fail_wr;

			if ((size_t)ret < bs)
				goto fail_wr_trunc;
		}
	}

	fragsz = fi->size % data->block_size;

	if (fragsz > 0) {
		if (fi->fragment == 0xFFFFFFFF ||
		    fi->fragment_offset == 0xFFFFFFFF) {
			memset(data->buffer, 0, fragsz);
		} else {
			if (data->frag == NULL)
				goto fail_frag;

			if (frag_reader_read(data->frag, fi->fragment,
					     fi->fragment_offset, data->buffer,
					     fragsz)) {
				return -1;
			}
		}

		ret = write_retry(outfd, data->buffer, fragsz);
		if (ret < 0)
			goto fail_wr;

		if ((size_t)ret < fragsz)
			goto fail_wr_trunc;
	}

	return 0;
fail_seek:
	perror("seek on squashfs");
	return -1;
fail_wr:
	perror("writing uncompressed block");
	return -1;
fail_wr_trunc:
	fputs("writing uncompressed block: truncated write\n", stderr);
	return -1;
fail_rd:
	perror("reading from squashfs");
	return -1;
fail_trunc:
	fputs("reading from squashfs: unexpected end of file\n", stderr);
	return -1;
fail_bs:
	fputs("found compressed block larger than block size\n", stderr);
	return -1;
fail_frag:
	fputs("file not a multiple of block size but no fragments available\n",
	      stderr);
	return -1;
}
