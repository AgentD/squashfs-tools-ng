/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "data_writer.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

struct data_writer_t {
	void *block;
	void *fragment;
	void *scratch;

	sqfs_fragment_t *fragments;
	size_t num_fragments;
	size_t max_fragments;
	size_t frag_offset;

	sqfs_super_t *super;
	compressor_t *cmp;
	int outfd;
};

static int write_compressed(data_writer_t *data, const void *in, size_t size,
			    uint32_t *outsize)
{
	ssize_t ret;

	ret = data->cmp->do_block(data->cmp, in, size, data->scratch,
				  data->super->block_size);
	if (ret < 0)
		return -1;

	if (ret > 0 && (size_t)ret < size) {
		size = ret;
		ret = write_retry(data->outfd, data->scratch, size);
		*outsize = size;
	} else {
		ret = write_retry(data->outfd, in, size);
		*outsize = size | (1 << 24);
	}

	if (ret < 0) {
		perror("writing to output file");
		return -1;
	}

	if ((size_t)ret < size) {
		fputs("write to output file truncated\n", stderr);
		return -1;
	}

	data->super->bytes_used += ret;
	return 0;
}

static int grow_fragment_table(data_writer_t *data)
{
	size_t newsz;
	void *new;

	if (data->num_fragments == data->max_fragments) {
		newsz = data->max_fragments ? data->max_fragments * 2 : 16;
		new = realloc(data->fragments,
			      sizeof(data->fragments[0]) * newsz);

		if (new == NULL) {
			perror("appending to fragment table");
			return -1;
		}

		data->max_fragments = newsz;
		data->fragments = new;
	}

	return 0;
}

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

int data_writer_flush_fragments(data_writer_t *data)
{
	uint64_t offset;
	uint32_t out;

	if (data->frag_offset == 0)
		return 0;

	if (grow_fragment_table(data))
		return -1;

	offset = data->super->bytes_used;

	if (write_compressed(data, data->fragment, data->frag_offset, &out))
		return -1;

	data->fragments[data->num_fragments].start_offset = htole64(offset);
	data->fragments[data->num_fragments].pad0 = 0;
	data->fragments[data->num_fragments].size = htole32(out);

	data->num_fragments += 1;
	data->frag_offset = 0;

	data->super->flags &= ~SQFS_FLAG_NO_FRAGMENTS;
	data->super->flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	return 0;
}

int write_data_from_fd(data_writer_t *data, file_info_t *fi, int infd)
{
	uint64_t count = fi->size;
	int blk_idx = 0;
	uint32_t out;
	ssize_t ret;
	size_t diff;

	fi->startblock = data->super->bytes_used;
	fi->sparse = 0;

	while (count != 0) {
		diff = count > (uint64_t)data->super->block_size ?
			data->super->block_size : count;

		ret = read_retry(infd, data->block, diff);
		if (ret < 0)
			goto fail_read;
		if ((size_t)ret < diff)
			goto fail_trunc;

		if (is_zero_block(data->block, diff)) {
			if (diff < data->super->block_size) {
				fi->fragment_offset = 0xFFFFFFFF;
				fi->fragment = 0xFFFFFFFF;
			} else {
				fi->blocksizes[blk_idx++] = 0;
			}
			fi->sparse += diff;
			count -= diff;
			continue;
		}

		if (diff < data->super->block_size) {
			if (data->frag_offset + diff > data->super->block_size) {
				if (data_writer_flush_fragments(data))
					return -1;
			}

			fi->fragment_offset = data->frag_offset;
			fi->fragment = data->num_fragments;

			memcpy((char *)data->fragment + data->frag_offset,
			       data->block, diff);
			data->frag_offset += diff;
		} else {
			if (write_compressed(data, data->block,
					     data->super->block_size, &out)) {
				return -1;
			}

			fi->blocksizes[blk_idx++] = out;
		}

		count -= diff;
	}

	return 0;
fail_read:
	fprintf(stderr, "read from %s: %s\n", fi->input_file, strerror(errno));
	return -1;
fail_trunc:
	fprintf(stderr, "%s: truncated read\n", fi->input_file);
	return -1;
}

data_writer_t *data_writer_create(sqfs_super_t *super, compressor_t *cmp,
				  int outfd)
{
	data_writer_t *data;

	data = calloc(1, sizeof(*data) + super->block_size * 3);
	if (data == NULL) {
		perror("creating data writer");
		return NULL;
	}

	data->block = (char *)data + sizeof(*data);
	data->fragment = (char *)data->block + super->block_size;
	data->scratch = (char *)data->fragment + super->block_size;

	data->super = super;
	data->cmp = cmp;
	data->outfd = outfd;
	return data;
}

void data_writer_destroy(data_writer_t *data)
{
	free(data->fragments);
	free(data);
}

int data_writer_write_fragment_table(data_writer_t *data)
{
	uint64_t start;

	if (data->num_fragments == 0) {
		data->super->fragment_entry_count = 0;
		data->super->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
		return 0;
	}

	data->super->fragment_entry_count = data->num_fragments;

	if (sqfs_write_table(data->outfd, data->super, data->fragments,
			     sizeof(data->fragments[0]), data->num_fragments,
			     &start, data->cmp)) {
		return -1;
	}

	data->super->fragment_table_start = start;
	return 0;
}
