/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <unistd.h>
#include <errno.h>

#include "util.h"

ssize_t read_retry(int fd, void *buffer, size_t size)
{
	ssize_t ret, total = 0;

	while (size > 0) {
		ret = read(fd, buffer, size);
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}
		if (ret == 0)
			break;

		total += ret;
		size -= ret;
		buffer = (char *)buffer + ret;
	}

	return total;
}
