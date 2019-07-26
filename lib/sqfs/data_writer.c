/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "data_writer.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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

	size_t devblksz;

	int block_idx;

	sqfs_super_t *super;
	compressor_t *cmp;
	int outfd;
};

static int write_compressed(data_writer_t *data, const void *in, size_t size,
			    uint32_t *outsize, int flags)
{
	ssize_t ret = 0;

	if (!(flags & DW_DONT_COMPRESS)) {
		ret = data->cmp->do_block(data->cmp, in, size, data->scratch,
					  data->super->block_size);
		if (ret < 0)
			return -1;
	}

	if (ret > 0 && (size_t)ret < size) {
		size = ret;
		in = data->scratch;
		*outsize = size;
	} else {
		*outsize = size | (1 << 24);
	}

	if (write_data("writing data block", data->outfd, in, size))
		return -1;

	data->super->bytes_used += size;
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

static int allign_file(data_writer_t *data)
{
	size_t diff = data->super->bytes_used % data->devblksz;

	if (diff == 0)
		return 0;

	if (padd_file(data->outfd, data->super->bytes_used, data->devblksz))
		return -1;

	data->super->bytes_used += data->devblksz - diff;
	return 0;
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

	if (write_compressed(data, data->fragment, data->frag_offset, &out, 0))
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

static file_info_t *fragment_by_chksum(file_info_t *fi, uint32_t chksum,
				       size_t frag_size, file_info_t *list,
				       size_t block_size)
{
	file_info_t *it;

	for (it = list; it != NULL; it = it->next) {
		if (it == fi) {
			it = NULL;
			break;
		}

		if (it->fragment == 0xFFFFFFFF)
			continue;

		if (it->fragment_offset == 0xFFFFFFFF)
			continue;

		if ((it->size % block_size) != frag_size)
			continue;

		if (it->fragment_chksum == chksum)
			break;
	}

	return it;
}

static size_t get_block_count(file_info_t *fi, size_t block_size)
{
	size_t count = fi->size / block_size;

	if ((fi->size % block_size != 0) &&
	    (fi->fragment == 0xFFFFFFFF ||
	     fi->fragment_offset == 0xFFFFFFFF)) {
		++count;
	}

	while (count > 0 && fi->blocks[count - 1].size == 0)
		--count;

	return count;
}

static size_t find_first_match(file_info_t *file, file_info_t *cmp,
			       size_t idx, size_t cmp_blk_count)
{
	size_t i;

	for (i = 0; i < cmp_blk_count; ++i) {
		if (memcmp(file->blocks + idx, cmp->blocks + i,
			   sizeof(file->blocks[idx])) == 0) {
			break;
		}
	}

	return i;
}

static uint64_t find_equal_blocks(file_info_t *file, file_info_t *list,
				  size_t block_size)
{
	size_t start, first_match, i, j, block_count, cmp_blk_count;
	uint64_t location;
	file_info_t *it;

	block_count = get_block_count(file, block_size);
	if (block_count == 0)
		return 0;

	for (start = 0; start < block_count; ++start) {
		if (file->blocks[start].size != 0)
			break;
	}

	location = 0;

	for (it = list; it != NULL; it = it->next) {
		if (it == file) {
			it = NULL;
			break;
		}

		/* XXX: list assumed to be processed in order, prevent us from
		   re-checking files we know are duplicates */
		if (it->startblock < location)
			continue;

		location = it->startblock;

		cmp_blk_count = get_block_count(it, block_size);
		if (cmp_blk_count == 0)
			continue;

		first_match = find_first_match(file, it, start, cmp_blk_count);
		if (first_match == cmp_blk_count)
			continue;

		i = start;
		j = first_match;

		while (i < block_count && j < cmp_blk_count) {
			if (file->blocks[i].size == 0) {
				++i;
				continue;
			}

			if (it->blocks[j].size == 0) {
				++j;
				continue;
			}

			if (memcmp(it->blocks + j, file->blocks + i,
				   sizeof(file->blocks[i])) != 0) {
				break;
			}

			++i;
			++j;
		}

		if (i == block_count)
			break;
	}

	if (it == NULL)
		return 0;

	location = it->startblock;

	for (i = 0; i < first_match; ++i)
		location += it->blocks[i].size & ((1 << 24) - 1);

	return location;
}

static int flush_data_block(data_writer_t *data, size_t size,
			    file_info_t *fi, int flags, file_info_t *list)
{
	uint32_t out, chksum;
	file_info_t *ref;

	if (is_zero_block(data->block, size)) {
		fi->blocks[data->block_idx].size = 0;
		fi->blocks[data->block_idx].chksum = 0;
		fi->sparse += size;
		data->block_idx++;
		return 0;
	}

	chksum = update_crc32(0, data->block, size);

	if (size < data->super->block_size && !(flags & DW_DONT_FRAGMENT)) {
		ref = fragment_by_chksum(fi, chksum, size, list,
					 data->super->block_size);

		if (ref != NULL) {
			fi->fragment_chksum = ref->fragment_chksum;
			fi->fragment_offset = ref->fragment_offset;
			fi->fragment = ref->fragment;
			return 0;
		}

		if (data->frag_offset + size > data->super->block_size) {
			if (data_writer_flush_fragments(data))
				return -1;
		}

		fi->fragment_chksum = chksum;
		fi->fragment_offset = data->frag_offset;
		fi->fragment = data->num_fragments;

		memcpy((char *)data->fragment + data->frag_offset,
		       data->block, size);
		data->frag_offset += size;
	} else {
		if (write_compressed(data, data->block, size, &out, flags))
			return -1;

		fi->blocks[data->block_idx].chksum = chksum;
		fi->blocks[data->block_idx].size = out;
		data->block_idx++;
	}

	return 0;
}

static int begin_file(data_writer_t *data, file_info_t *fi,
		      int flags, off_t *start)
{
	*start = lseek(data->outfd, 0, SEEK_CUR);
	if (*start == (off_t)-1)
		goto fail_seek;

	if ((flags & DW_ALLIGN_DEVBLK) && allign_file(data) != 0)
		return -1;

	fi->startblock = data->super->bytes_used;
	fi->sparse = 0;
	data->block_idx = 0;
	return 0;
fail_seek:
	perror("querying current position on squashfs image");
	return -1;
}

static int end_file(data_writer_t *data, file_info_t *fi,
		    off_t start, int flags, file_info_t *list)
{
	uint64_t ref;

	if ((flags & DW_ALLIGN_DEVBLK) && allign_file(data) != 0)
		return -1;

	ref = find_equal_blocks(fi, list, data->super->block_size);

	if (ref > 0) {
		fi->startblock = ref;

		if (lseek(data->outfd, start, SEEK_SET) == (off_t)-1)
			goto fail_seek;

		if (ftruncate(data->outfd, start))
			goto fail_truncate;
	}
	return 0;
fail_seek:
	perror("seeking on squashfs image after file deduplication");
	return -1;
fail_truncate:
	perror("truncating squashfs image after file deduplication");
	return -1;
}

int write_data_from_fd(data_writer_t *data, file_info_t *fi,
		       int infd, int flags, file_info_t *list)
{
	uint64_t count;
	size_t diff;
	off_t start;

	if (begin_file(data, fi, flags, &start))
		return -1;

	for (count = fi->size; count != 0; count -= diff) {
		diff = count > (uint64_t)data->super->block_size ?
			data->super->block_size : count;

		if (read_data(fi->input_file, infd, data->block, diff))
			return -1;

		if (flush_data_block(data, diff, fi, flags, list))
			return -1;
	}

	return end_file(data, fi, start, flags, list);
}

int write_data_from_fd_condensed(data_writer_t *data, file_info_t *fi,
				 int infd, sparse_map_t *map, int flags,
				 file_info_t *list)
{
	size_t start, count, diff;
	sparse_map_t *m;
	uint64_t offset;
	off_t location;

	if (begin_file(data, fi, flags, &location))
		return -1;

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

			if (read_data(fi->input_file, infd,
				      (char *)data->block + start, count)) {
				return -1;
			}

			map = map->next;
		}

		if (flush_data_block(data, diff, fi, flags, list))
			return -1;
	}

	return end_file(data, fi, location, flags, list);
fail_map_size:
	fprintf(stderr, "%s: sparse file map spans beyond file size\n",
		fi->input_file);
	return -1;
fail_map:
	fprintf(stderr,
		"%s: sparse file map is unordered or self overlapping\n",
		fi->input_file);
	return -1;
}

data_writer_t *data_writer_create(sqfs_super_t *super, compressor_t *cmp,
				  int outfd, size_t devblksize)
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
	data->devblksz = devblksize;
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
	size_t size;
	int ret;

	if (data->num_fragments == 0) {
		data->super->fragment_entry_count = 0;
		data->super->fragment_table_start = 0xFFFFFFFFFFFFFFFFUL;
		return 0;
	}

	size = sizeof(data->fragments[0]) * data->num_fragments;
	ret = sqfs_write_table(data->outfd, data->super, data->cmp,
			       data->fragments, size, &start);
	if (ret)
		return -1;

	data->super->fragment_entry_count = data->num_fragments;
	data->super->fragment_table_start = start;
	return 0;
}
