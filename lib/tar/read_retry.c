/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * read_retry.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "tar.h"
#include "internal.h"

int read_retry(const char *errstr, FILE *fp, void *buffer, size_t size)
{
	size_t ret;

	while (size > 0) {
		if (ferror(fp)) {
			fprintf(stderr, "%s: error reading from file\n",
				errstr);
			return -1;
		}

		if (feof(fp)) {
			fprintf(stderr, "%s: short read\n", errstr);
			return -1;
		}

		ret = fread(buffer, 1, size, fp);
		size -= ret;
		buffer = (char *)buffer + ret;
	}

	return 0;
}

char *record_to_memory(FILE *fp, sqfs_u64 size)
{
	char *buffer = malloc(size + 1);

	if (buffer == NULL)
		goto fail_errno;

	if (read_retry("reading tar record", fp, buffer, size))
		goto fail;

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
