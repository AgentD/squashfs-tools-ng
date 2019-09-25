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
#include "sqfs/block.h"
#include "util.h"

#include <stdlib.h>

int sqfs_file_create_block(sqfs_file_t *file, uint64_t offset,
			   size_t size, sqfs_inode_generic_t *inode,
			   uint32_t flags, sqfs_block_t **out)
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

	blk->inode = inode;
	blk->size = size;
	blk->flags = flags;

	*out = blk;
	return 0;
}
