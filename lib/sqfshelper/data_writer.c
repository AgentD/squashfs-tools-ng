/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

int write_data_from_file(sqfs_data_writer_t *data, sqfs_inode_generic_t *inode,
			 sqfs_file_t *file, size_t block_size, int flags)
{
	uint64_t filesz, offset;
	sqfs_block_t *blk;
	size_t diff;
	int ret;
	(void)flags;

	if (sqfs_data_writer_begin_file(data, inode, 0))
		return -1;

	sqfs_inode_get_file_size(inode, &filesz);

	for (offset = 0; offset < filesz; offset += diff) {
		if (filesz - offset > (uint64_t)block_size) {
			diff = block_size;
		} else {
			diff = filesz - offset;
		}

		ret = sqfs_file_create_block(file, offset, diff,
					     NULL, 0, &blk);

		if (ret)
			return -1;

		if (sqfs_data_writer_enqueue(data, blk))
			return -1;
	}

	return sqfs_data_writer_end_file(data);
}
