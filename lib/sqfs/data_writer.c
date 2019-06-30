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

	int block_idx;

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

static int flush_data_block(data_writer_t *data, size_t size, file_info_t *fi)
{
	uint32_t out;

	if (is_zero_block(data->block, size)) {
		if (size < data->super->block_size) {
			fi->fragment_offset = 0xFFFFFFFF;
			fi->fragment = 0xFFFFFFFF;
		} else {
			fi->blocksizes[data->block_idx++] = 0;
		}

		fi->sparse += size;
		return 0;
	}

	if (size < data->super->block_size) {
		if (data->frag_offset + size > data->super->block_size) {
			if (data_writer_flush_fragments(data))
				return -1;
		}

		fi->fragment_offset = data->frag_offset;
		fi->fragment = data->num_fragments;

		memcpy((char *)data->fragment + data->frag_offset,
		       data->block, size);
		data->frag_offset += size;
	} else {
		if (write_compressed(data, data->block, size, &out))
			return -1;

		fi->blocksizes[data->block_idx++] = out;
	}

	return 0;
}

int write_data_from_fd(data_writer_t *data, file_info_t *fi, int infd)
{
	uint64_t count;
	ssize_t ret;
	size_t diff;

	fi->startblock = data->super->bytes_used;
	fi->sparse = 0;
	data->block_idx = 0;

	for (count = fi->size; count != 0; count -= diff) {
		diff = count > (uint64_t)data->super->block_size ?
			data->super->block_size : count;

		ret = read_retry(infd, data->block, diff);
		if (ret < 0)
			goto fail_read;
		if ((size_t)ret < diff)
			goto fail_trunc;

		if (flush_data_block(data, diff, fi))
			return -1;
	}

	return 0;
fail_read:
	perror(fi->input_file);
	return -1;
fail_trunc:
	fprintf(stderr, "%s: truncated read\n", fi->input_file);
	return -1;
}

int write_data_from_fd_condensed(data_writer_t *data, file_info_t *fi,
				 int infd, sparse_map_t *map)
{
	size_t start, count, diff;
	sparse_map_t *m;
	uint64_t offset;
	ssize_t ret;

	fi->startblock = data->super->bytes_used;
	fi->sparse = 0;
	data->block_idx = 0;

	if (map != NULL) {
		offset = map->offset;

		for (m = map; m != NULL; m = m->next) {
			if (m->offset < offset)
				goto fail_map;
			offset = m->offset + m->count;
		}

		if (offset > fi->size)
			goto fail_map_size;
	}

	for (offset = 0; offset < fi->size; offset += diff) {
		if (fi->size - offset >= (uint64_t)data->super->block_size) {
			diff = data->super->block_size;
		} else {
			diff = fi->size - offset;
		}

		memset(data->block, 0, diff);

		while (map != NULL && map->offset < offset + diff) {
			start = 0;
			count = map->count;

			if (map->offset < offset)
				count -= offset - map->offset;

			if (map->offset > offset)
				start = map->offset - offset;

			if (start + count > diff)
				count = diff - start;

			ret = read_retry(infd, (char *)data->block + start,
					 count);
			if (ret < 0)
				goto fail_read;
			if ((size_t)ret < count)
				goto fail_trunc;

			map = map->next;
		}

		if (flush_data_block(data, diff, fi))
			return -1;
	}

	return 0;
fail_map_size:
	fprintf(stderr, "%s: sparse file map spans beyond file size\n",
		fi->input_file);
	return -1;
fail_map:
	fprintf(stderr,
		"%s: sparse file map is unordered or self overlapping\n",
		fi->input_file);
	return -1;
fail_read:
	perror(fi->input_file);
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
