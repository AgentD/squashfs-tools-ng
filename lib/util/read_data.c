/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

#include "util.h"

int read_data(const char *errstr, int fd, void *buffer, size_t size)
{
	ssize_t ret;

	while (size > 0) {
		ret = read(fd, buffer, size);
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
	}

	return 0;
}
