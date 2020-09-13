/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * record_to_memory.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "tar.h"
#include "internal.h"

char *record_to_memory(istream_t *fp, size_t size)
{
	char *buffer = malloc(size + 1);
	int ret;

	if (buffer == NULL)
		goto fail_errno;

	ret = istream_read(fp, buffer, size);
	if (ret < 0)
		goto fail;

	if ((size_t)ret < size) {
		fputs("Reading tar record: unexpected end-of-file.\n", stderr);
		goto fail;
	}

	if (skip_padding(fp, size))
		goto fail;

	buffer[size] = '\0';
	return buffer;
fail_errno:
	perror("reading tar record");
	goto fail;
fail:
	free(buffer);
	return NULL;
}
