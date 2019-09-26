/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

static uint8_t buffer[4096];

int write_data_from_file(sqfs_data_writer_t *data, sqfs_inode_generic_t *inode,
			 sqfs_file_t *file, int flags)
{
	uint64_t filesz, offset;
	size_t diff;
	int ret;

	if (sqfs_data_writer_begin_file(data, inode, flags))
		return -1;

	sqfs_inode_get_file_size(inode, &filesz);

	for (offset = 0; offset < filesz; offset += diff) {
		if (filesz - offset > sizeof(buffer)) {
			diff = sizeof(buffer);
		} else {
			diff = filesz - offset;
		}

		ret = file->read_at(file, offset, buffer, diff);
		if (ret)
			return ret;

		ret = sqfs_data_writer_append(data, buffer, diff);
		if (ret)
			return ret;
	}

	return sqfs_data_writer_end_file(data);
}
