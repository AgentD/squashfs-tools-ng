/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * read_data_at.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "util.h"

int read_data_at(const char *errstr, off_t location, int fd,
		 void *buffer, size_t size)
{
	ssize_t ret;

	while (size > 0) {
		ret = pread(fd, buffer, size, location);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror(errstr);
			return -1;
		}
		if (ret == 0) {
			fprintf(stderr, "%s: short read\n", errstr);
			return -1;
		}

		size -= ret;
		buffer = (char *)buffer + ret;
		location += ret;
	}

	return 0;
}
