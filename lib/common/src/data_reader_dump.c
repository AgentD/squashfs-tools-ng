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
			  sqfs_ostream_t *fp, size_t block_size)
{
	size_t i, diff, chunk_size;
	sqfs_u64 filesz;
	sqfs_u8 *chunk;
	int err;

	sqfs_inode_get_file_size(inode, &filesz);

	for (i = 0; i < sqfs_inode_get_file_block_count(inode); ++i) {
		diff = (filesz < block_size) ? filesz : block_size;

		if (SQFS_IS_SPARSE_BLOCK(inode->extra[i])) {
			err = fp->append(fp, NULL, diff);
		} else {
			err = sqfs_data_reader_get_block(data, inode, i,
							 &chunk_size, &chunk);
			if (err) {
				sqfs_perror(name, "reading data block", err);
				return -1;
			}

			err = fp->append(fp, chunk, chunk_size);
			free(chunk);
		}

		if (err)
			goto fail_io;

		filesz -= diff;
	}

	if (filesz > 0) {
		err = sqfs_data_reader_get_fragment(data, inode,
						    &chunk_size, &chunk);
		if (err) {
			sqfs_perror(name, "reading fragment block", err);
			return -1;
		}

		err = fp->append(fp, chunk, chunk_size);
		free(chunk);

		if (err)
			goto fail_io;
	}

	return 0;
fail_io:
	sqfs_perror(fp->get_filename(fp), "writing data block", err);
	return -1;
}
