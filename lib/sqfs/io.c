/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * io.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/io.h"
#include "sqfs/error.h"
#include "sqfs/block_processor.h"
#include "util.h"

#include <stdlib.h>

int sqfs_file_create_block(sqfs_file_t *file, uint64_t offset,
			   size_t size, void *user, uint32_t flags,
			   sqfs_block_t **out)
{
	sqfs_block_t *blk = alloc_flex(sizeof(*blk), 1, size);
	int err;

	if (blk == NULL)
		return SQFS_ERROR_ALLOC;

	err = file->read_at(file, offset, blk->data, size);
	if (err) {
		free(blk);
		return err;
	}

	blk->user = user;
	blk->size = size;
	blk->flags = flags;

	*out = blk;
	return 0;
}

int sqfs_file_create_block_dense(sqfs_file_t *file, uint64_t offset,
				 size_t size, void *user, uint32_t flags,
				 const sqfs_sparse_map_t *map,
				 sqfs_block_t **out)
{
	sqfs_block_t *blk = alloc_flex(sizeof(*blk), 1, size);
	size_t dst_start, diff, count;
	const sqfs_sparse_map_t *it;
	uint64_t poffset, src_start;
	int err;

	if (blk == NULL)
		return SQFS_ERROR_ALLOC;

	poffset = 0;

	for (it = map; it != NULL; it = it->next) {
		if (it->offset + it->count <= offset) {
			poffset += it->count;
			continue;
		}

		if (it->offset >= offset + size) {
			poffset += it->count;
			continue;
		}

		count = size;

		if (offset + count >= it->offset + it->count)
			count = it->offset + it->count - offset;

		if (it->offset < offset) {
			diff = offset - it->offset;

			src_start = poffset + diff;
			dst_start = 0;
			count -= diff;
		} else if (it->offset > offset) {
			diff = it->offset - offset;

			src_start = poffset;
			dst_start = diff;
		} else {
			src_start = poffset;
			dst_start = 0;
		}

		err = file->read_at(file, src_start,
				    blk->data + dst_start, count);
		if (err) {
			free(blk);
			return err;
		}

		poffset += it->count;
	}

	blk->user = user;
	blk->size = size;
	blk->flags = flags;

	*out = blk;
	return 0;
}
