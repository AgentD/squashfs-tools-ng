/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * padd_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "sqfs/io.h"
#include "util.h"

#include <stdlib.h>
#include <stdio.h>

int padd_file(int outfd, sqfs_u64 size, size_t blocksize)
{
	size_t padd_sz = size % blocksize;
	int status = -1;
	sqfs_u8 *buffer;

	if (padd_sz == 0)
		return 0;

	padd_sz = blocksize - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL)
		goto fail_errno;

	if (write_data("padding output file to block size",
		       outfd, buffer, padd_sz)) {
		goto out;
	}

	status = 0;
out:
	free(buffer);
	return status;
fail_errno:
	perror("padding output file to block size");
	goto out;
}

int padd_sqfs(sqfs_file_t *file, sqfs_u64 size, size_t blocksize)
{
	size_t padd_sz = size % blocksize;
	int status = -1;
	sqfs_u8 *buffer;

	if (padd_sz == 0)
		return 0;

	padd_sz = blocksize - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL)
		goto fail_errno;

	if (file->write_at(file, file->get_size(file),
			   buffer, padd_sz)) {
		goto fail_errno;
	}

	status = 0;
out:
	free(buffer);
	return status;
fail_errno:
	perror("padding output file to block size");
	goto out;
}
