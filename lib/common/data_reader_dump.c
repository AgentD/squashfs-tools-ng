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

static int append_block(FILE *fp, const sqfs_u8 *data, size_t size)
{
	const sqfs_u8 *ptr = data;
	size_t ret;

	while (size > 0) {
		if (ferror(fp)) {
			fputs("writing data block: error writing to file\n",
			      stderr);
		}

		if (feof(fp)) {
			fputs("writing data block: unexpected end of file\n",
			      stderr);
		}

		ret = fwrite(ptr, 1, size, fp);
		ptr += ret;
		size -= ret;
	}

	return 0;
}

int sqfs_data_reader_dump(const char *name, sqfs_data_reader_t *data,
			  const sqfs_inode_generic_t *inode,
			  FILE *fp, size_t block_size, bool allow_sparse)
{
	size_t i, diff, chunk_size;
	sqfs_u64 filesz;
	sqfs_u8 *chunk;
	int err;

	sqfs_inode_get_file_size(inode, &filesz);

#if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L)
	if (allow_sparse) {
		int fd = fileno(fp);

		if (ftruncate(fd, filesz))
			goto fail_sparse;
	}
#else
	allow_sparse = false;
#endif

	for (i = 0; i < sqfs_inode_get_file_block_count(inode); ++i) {
		diff = (filesz < block_size) ? filesz : block_size;

		if (SQFS_IS_SPARSE_BLOCK(inode->extra[i]) && allow_sparse) {
			if (fseek(fp, diff, SEEK_CUR) < 0)
				goto fail_sparse;
		} else {
			err = sqfs_data_reader_get_block(data, inode, i,
							 &chunk_size, &chunk);
			if (err) {
				sqfs_perror(name, "reading data block", err);
				return -1;
			}

			err = append_block(fp, chunk, chunk_size);
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

		if (append_block(fp, chunk, chunk_size)) {
			free(chunk);
			return -1;
		}

		free(chunk);
	}

	return 0;
fail_sparse:
	perror("creating sparse output file");
	return -1;
}
