/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * data_reader_dump.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

int sqfs_data_reader_dump(const char *name, sqfs_data_reader_t *data,
			  const sqfs_inode_generic_t *inode,
			  ostream_t *fp, size_t block_size)
{
	size_t i, diff, chunk_size;
	sqfs_u64 filesz;
	sqfs_u8 *chunk;
	int err;

	sqfs_inode_get_file_size(inode, &filesz);

	for (i = 0; i < sqfs_inode_get_file_block_count(inode); ++i) {
		diff = (filesz < block_size) ? filesz : block_size;

		if (SQFS_IS_SPARSE_BLOCK(inode->extra[i])) {
			if (ostream_append_sparse(fp, diff))
				return -1;
		} else {
			err = sqfs_data_reader_get_block(data, inode, i,
							 &chunk_size, &chunk);
			if (err) {
				sqfs_perror(name, "reading data block", err);
				return -1;
			}

			err = ostream_append(fp, chunk, chunk_size);
			free(chunk);

			if (err)
				return -1;
		}

		filesz -= diff;
	}

	if (filesz > 0) {
		err = sqfs_data_reader_get_fragment(data, inode,
						    &chunk_size, &chunk);
		if (err) {
			sqfs_perror(name, "reading fragment block", err);
			return -1;
		}

		err = ostream_append(fp, chunk, chunk_size);
		free(chunk);

		if (err)
			return -1;
	}

	return 0;
}
