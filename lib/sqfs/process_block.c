/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * process_block.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#define SQFS_BUILDING_DLL
#include "config.h"

#include "sqfs/block_processor.h"
#include "sqfs/compress.h"
#include "util.h"

#include <string.h>
#include <zlib.h>

int sqfs_block_process(sqfs_block_t *block, sqfs_compressor_t *cmp,
		       uint8_t *scratch, size_t scratch_size)
{
	ssize_t ret;

	if (!(block->flags & SQFS_BLK_DONT_CHECKSUM))
		block->checksum = crc32(0, block->data, block->size);

	if (!(block->flags & SQFS_BLK_DONT_COMPRESS)) {
		ret = cmp->do_block(cmp, block->data, block->size,
				    scratch, scratch_size);

		if (ret < 0)
			return -1;

		if (ret > 0) {
			memcpy(block->data, scratch, ret);
			block->size = ret;
			block->flags |= SQFS_BLK_IS_COMPRESSED;
		}
	}

	return 0;
}
