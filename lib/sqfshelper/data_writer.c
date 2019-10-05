/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_writer.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "highlevel.h"
#include "util.h"

static sqfs_u8 buffer[4096];

int write_data_from_file(const char *filename, sqfs_data_writer_t *data,
			 sqfs_inode_generic_t *inode, sqfs_file_t *file,
			 int flags)
{
	sqfs_u64 filesz, offset;
	size_t diff;
	int ret;

	ret = sqfs_data_writer_begin_file(data, inode, flags);
	if (ret) {
		sqfs_perror(filename, "beginning file data blocks", ret);
		return -1;
	}

	sqfs_inode_get_file_size(inode, &filesz);

	for (offset = 0; offset < filesz; offset += diff) {
		if (filesz - offset > sizeof(buffer)) {
			diff = sizeof(buffer);
		} else {
			diff = filesz - offset;
		}

		ret = file->read_at(file, offset, buffer, diff);
		if (ret) {
			sqfs_perror(filename, "reading file range", ret);
			return -1;
		}

		ret = sqfs_data_writer_append(data, buffer, diff);
		if (ret) {
			sqfs_perror(filename, "packing file data", ret);
			return -1;
		}
	}

	ret = sqfs_data_writer_end_file(data);
	if (ret) {
		sqfs_perror(filename, "finishing file data", ret);
		return -1;
	}

	return 0;
}
