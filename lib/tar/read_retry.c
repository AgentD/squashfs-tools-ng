/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * read_retry.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "tar.h"

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
