/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * padd_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "tar.h"

#include <stdlib.h>
#include <stdio.h>

int padd_file(ostream_t *fp, sqfs_u64 size)
{
	size_t padd_sz = size % TAR_RECORD_SIZE;
	int status = -1;
	sqfs_u8 *buffer;

	if (padd_sz == 0)
		return 0;

	padd_sz = TAR_RECORD_SIZE - padd_sz;

	buffer = calloc(1, padd_sz);
	if (buffer == NULL)
		goto fail_errno;

	if (ostream_append(fp, buffer, padd_sz))
		goto out;

	status = 0;
out:
	free(buffer);
	return status;
fail_errno:
	perror("padding output file to block size");
	goto out;
}
