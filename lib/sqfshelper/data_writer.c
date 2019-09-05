/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/block_processor.h"
#include "data_writer.h"
#include "highlevel.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <zlib.h>

struct data_writer_t {
	block_t *frag_block;
	sqfs_fragment_t *fragments;
	size_t num_fragments;
	size_t max_fragments;

	size_t devblksz;
	uint64_t bytes_written;
	off_t start;

	block_processor_t *proc;
	compressor_t *cmp;
	file_info_t *list;
	sqfs_super_t *super;
	int outfd;
};

enum {
	BLK_FIRST_BLOCK = BLK_USER,
	BLK_LAST_BLOCK = BLK_USER << 1,
	BLK_ALLIGN = BLK_USER << 2,
	BLK_FRAGMENT_BLOCK = BLK_USER << 3,
};

static int save_position(data_writer_t *data)
{
	data->bytes_written = data->super->bytes_used;
	data->start = lseek(data->outfd, 0, SEEK_CUR);

	if (data->start == (off_t)-1) {
		perror("querying current position on squashfs image");
		return -1;
	}

	return 0;
}

static int restore_position(data_writer_t *data)
{
	if (lseek(data->outfd, data->start, SEEK_SET) == (off_t)-1)
		goto fail_seek;

	if (ftruncate(data->outfd, data->start))
		goto fail_truncate;

	data->super->bytes_used = data->bytes_written;
	return 0;
fail_seek:
	perror("seeking on squashfs image after file deduplication");
	return -1;
fail_truncate:
	perror("truncating squashfs image after file deduplication");
	return -1;
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

static int block_callback(void *user, block_t *blk)
{
	file_info_t *fi = blk->user;
	data_writer_t *data = user;
	uint64_t ref, offset;
	uint32_t out;

	if (blk->flags & BLK_FIRST_BLOCK) {
		if (save_position(data))
			return -1;

		if ((blk->flags & BLK_ALLIGN) && allign_file(data) != 0)
			return -1;

		fi->startblock = data->super->bytes_used;
	}

	if (blk->size == 0)
		goto skip_sentinel;

	out = blk->size;
	if (!(blk->flags & BLK_IS_COMPRESSED))
		out |= 1 << 24;

	if (blk->flags & BLK_FRAGMENT_BLOCK) {
		offset = htole64(data->super->bytes_used);
		data->fragments[blk->index].start_offset = offset;
		data->fragments[blk->index].pad0 = 0;
		data->fragments[blk->index].size = htole32(out);

		data->super->flags &= ~SQFS_FLAG_NO_FRAGMENTS;
		data->super->flags |= SQFS_FLAG_ALWAYS_FRAGMENTS;
	} else {
		fi->blocks[blk->index].chksum = blk->checksum;
		fi->blocks[blk->index].size = htole32(out);
	}

	if (write_data("writing data block", data->outfd,
		       blk->data, blk->size)) {
		return -1;
	}

	data->super->bytes_used += blk->size;

skip_sentinel:
	if (blk->flags & BLK_LAST_BLOCK) {
		if ((blk->flags & BLK_ALLIGN) && allign_file(data) != 0)
			return -1;

		ref = find_equal_blocks(fi, data->list,
					data->super->block_size);
		if (ref > 0) {
			fi->startblock = ref;
			fi->flags |= FILE_FLAG_BLOCKS_ARE_DUPLICATE;

			if (restore_position(data))
				return -1;
		}
	}

	return 0;
}

/*****************************************************************************/

static int flush_fragment_block(data_writer_t *data)
{
	size_t newsz;
	void *new;
	int ret;

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

	data->frag_block->index = data->num_fragments++;

	ret = block_processor_enqueue(data->proc, data->frag_block);
	data->frag_block = NULL;
	return ret;
}

static int store_fragment(data_writer_t *data, block_t *frag)
{
	file_info_t *fi = frag->user;
	size_t size;

	if (data->frag_block != NULL) {
		size = data->frag_block->size + frag->size;

		if (size > data->super->block_size) {
			if (flush_fragment_block(data))
				goto fail;
		}
	}

	if (data->frag_block == NULL) {
		size = sizeof(block_t) + data->super->block_size;

		data->frag_block = calloc(1, size);
		if (data->frag_block == NULL) {
			perror("creating fragment block");
			goto fail;
		}

		data->frag_block->flags = BLK_DONT_CHECKSUM;
		data->frag_block->flags |= BLK_FRAGMENT_BLOCK;
	}

	fi->fragment_offset = data->frag_block->size;
	fi->fragment = data->num_fragments;

	data->frag_block->flags |= (frag->flags & BLK_DONT_COMPRESS);
	memcpy(data->frag_block->data + data->frag_block->size,
	       frag->data, frag->size);

	data->frag_block->size += frag->size;
	free(frag);
	return 0;
fail:
	free(frag);
	return -1;
}

static bool is_zero_block(unsigned char *ptr, size_t size)
{
	return ptr[0] == 0 && memcmp(ptr, ptr + 1, size - 1) == 0;
}

static int handle_fragment(data_writer_t *data, block_t *blk)
{
	file_info_t *fi = blk->user, *ref;

	fi->fragment_chksum = crc32(0, blk->data, blk->size);

	ref = fragment_by_chksum(fi, fi->fragment_chksum, blk->size,
				 data->list, data->super->block_size);

	if (ref != NULL) {
		fi->fragment_offset = ref->fragment_offset;
		fi->fragment = ref->fragment;
		fi->flags |= FILE_FLAG_FRAGMENT_IS_DUPLICATE;
		free(blk);
		return 0;
	}

	return store_fragment(data, blk);
}

static int add_sentinel_block(data_writer_t *data, file_info_t *fi,
			      uint32_t flags)
{
	block_t *blk = calloc(1, sizeof(*blk));

	if (blk == NULL) {
		perror("creating sentinel block");
		return -1;
	}

	blk->user = fi;
	blk->flags = BLK_DONT_COMPRESS | BLK_DONT_CHECKSUM | flags;

	return block_processor_enqueue(data->proc, blk);
}

static block_t *create_block(file_info_t *fi, int fd, size_t size,
			     uint32_t flags)
{
	block_t *blk = alloc_flex(sizeof(*blk), 1, size);

	if (blk == NULL) {
		perror(fi->input_file);
		return NULL;
	}

	if (fd >= 0) {
		if (read_data(fi->input_file, fd, blk->data, size)) {
			free(blk);
			return NULL;
		}
	}

	blk->size = size;
	blk->user = fi;
	blk->flags = flags;
	return blk;
}

int write_data_from_fd(data_writer_t *data, file_info_t *fi,
		       int infd, int flags)
{
	uint32_t blk_flags = BLK_FIRST_BLOCK;
	uint64_t file_size = fi->size;
	size_t diff, i = 0;
	block_t *blk;

	if (flags & DW_DONT_COMPRESS)
		blk_flags |= BLK_DONT_COMPRESS;

	if (flags & DW_ALLIGN_DEVBLK)
		blk_flags |= BLK_ALLIGN;

	fi->next = data->list;
	data->list = fi;

	for (; file_size > 0; file_size -= diff) {
		if (file_size > data->super->block_size) {
			diff = data->super->block_size;
		} else {
			diff = file_size;
		}

		blk = create_block(fi, infd, diff, blk_flags);
		if (blk == NULL)
			return -1;

		blk->index = i++;

		if (is_zero_block(blk->data, blk->size)) {
			fi->blocks[blk->index].chksum = 0;
			fi->blocks[blk->index].size = 0;
			free(blk);
			continue;
		}

		if (diff < data->super->block_size &&
		    !(flags & DW_DONT_FRAGMENT)) {
			fi->flags |= FILE_FLAG_HAS_FRAGMENT;

			if (!(blk_flags & (BLK_FIRST_BLOCK | BLK_LAST_BLOCK))) {
				blk_flags |= BLK_LAST_BLOCK;

				if (add_sentinel_block(data, fi, blk_flags)) {
					free(blk);
					return -1;
				}
			}

			if (handle_fragment(data, blk))
				return -1;
		} else {
			if (block_processor_enqueue(data->proc, blk))
				return -1;

			blk_flags &= ~BLK_FIRST_BLOCK;
		}
	}

	if (!(blk_flags & (BLK_FIRST_BLOCK | BLK_LAST_BLOCK))) {
		blk_flags |= BLK_LAST_BLOCK;

		if (add_sentinel_block(data, fi, blk_flags))
			return -1;
	}

	return 0;
}

static int check_map_valid(const sparse_map_t *map, file_info_t *fi)
{
	const sparse_map_t *m;
	uint64_t offset;

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
}

static int get_sparse_block(block_t *blk, file_info_t *fi, int infd,
			    sparse_map_t **sparse_map, uint64_t offset,
			    size_t diff)
{
	sparse_map_t *map = *sparse_map;
	size_t start, count;

	while (map != NULL && map->offset < offset + diff) {
		start = 0;
		count = map->count;

		if (map->offset < offset)
			count -= offset - map->offset;

		if (map->offset > offset)
			start = map->offset - offset;

		if (start + count > diff)
			count = diff - start;

		if (read_data(fi->input_file, infd, blk->data + start, count))
			return -1;

		map = map->next;
	}

	*sparse_map = map;
	return 0;
}

int write_data_from_fd_condensed(data_writer_t *data, file_info_t *fi,
				 int infd, sparse_map_t *map, int flags)
{
	uint32_t blk_flags = BLK_FIRST_BLOCK;
	size_t diff, i = 0;
	uint64_t offset;
	block_t *blk;

	if (check_map_valid(map, fi))
		return -1;

	if (flags & DW_DONT_COMPRESS)
		blk_flags |= BLK_DONT_COMPRESS;

	if (flags & DW_ALLIGN_DEVBLK)
		blk_flags |= BLK_ALLIGN;

	for (offset = 0; offset < fi->size; offset += diff) {
		if (fi->size - offset > (uint64_t)data->super->block_size) {
			diff = data->super->block_size;
		} else {
			diff = fi->size - offset;
		}

		blk = alloc_flex(sizeof(*blk), 1, diff);
		blk->size = diff;
		blk->index = i++;
		blk->user = fi;
		blk->flags = blk_flags;

		if (get_sparse_block(blk, fi, infd, &map, offset, diff)) {
			free(blk);
			return -1;
		}

		if (is_zero_block(blk->data, blk->size)) {
			fi->blocks[blk->index].chksum = 0;
			fi->blocks[blk->index].size = 0;
			free(blk);
			continue;
		}

		if (diff < data->super->block_size &&
		    !(flags & DW_DONT_FRAGMENT)) {
			fi->flags |= FILE_FLAG_HAS_FRAGMENT;

			if (!(blk_flags & (BLK_FIRST_BLOCK | BLK_LAST_BLOCK))) {
				blk_flags |= BLK_LAST_BLOCK;

				if (add_sentinel_block(data, fi, blk_flags)) {
					free(blk);
					return -1;
				}
			}

			if (handle_fragment(data, blk))
				return -1;
		} else {
			if (block_processor_enqueue(data->proc, blk))
				return -1;

			blk_flags &= ~BLK_FIRST_BLOCK;
		}
	}

	if (!(blk_flags & (BLK_FIRST_BLOCK | BLK_LAST_BLOCK))) {
		blk_flags |= BLK_LAST_BLOCK;

		if (add_sentinel_block(data, fi, blk_flags))
			return -1;
	}

	return 0;
}

data_writer_t *data_writer_create(sqfs_super_t *super, compressor_t *cmp,
				  int outfd, size_t devblksize,
				  unsigned int num_jobs)
{
	data_writer_t *data = calloc(1, sizeof(*data));

	if (data == NULL) {
		perror("creating data writer");
		return NULL;
	}

	data->proc = block_processor_create(super->block_size, cmp, num_jobs,
					    data, block_callback);
	data->cmp = cmp;
	data->super = super;
	data->outfd = outfd;
	data->devblksz = devblksize;
	return data;
}

void data_writer_destroy(data_writer_t *data)
{
	block_processor_destroy(data->proc);
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

int data_writer_sync(data_writer_t *data)
{
	if (data->frag_block != NULL) {
		if (flush_fragment_block(data))
			return -1;
	}

	return block_processor_finish(data->proc);
}
