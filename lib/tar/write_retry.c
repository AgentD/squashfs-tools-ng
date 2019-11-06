/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * write_retry.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "tar.h"

int write_retry(const char *errstr, FILE *fp, const void *data, size_t size)
{
	size_t ret;

	while (size > 0) {
		if (feof(fp)) {
			fprintf(stderr, "%s: write truncated\n", errstr);
			return -1;
		}

		if (ferror(fp)) {
			fprintf(stderr, "%s: error writing to file\n", errstr);
			return -1;
		}

		ret = fwrite(data, 1, size, fp);
		data = (const char *)data + ret;
		size -= ret;
	}

	return 0;
}
