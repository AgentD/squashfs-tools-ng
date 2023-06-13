/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * record_to_memory.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "tar/tar.h"
#include "internal.h"
#include <stdlib.h>

char *record_to_memory(sqfs_istream_t *fp, size_t size)
{
	char *buffer = malloc(size + 1);
	int ret;

	if (buffer == NULL)
		goto fail_errno;

	ret = sqfs_istream_read(fp, buffer, size);
	if (ret < 0) {
		sqfs_perror(fp->get_filename(fp), "reading tar record", ret);
		goto fail;
	}

	if ((size_t)ret < size) {
		fputs("Reading tar record: unexpected end-of-file.\n", stderr);
		goto fail;
	}

	if (size % 512) {
		ret = sqfs_istream_skip(fp, 512 - (size % 512));
		if (ret) {
			sqfs_perror(fp->get_filename(fp),
				    "skipping tar padding", ret);
			goto fail;
		}
	}

	buffer[size] = '\0';
	return buffer;
fail_errno:
	perror("reading tar record");
	goto fail;
fail:
	free(buffer);
	return NULL;
}
