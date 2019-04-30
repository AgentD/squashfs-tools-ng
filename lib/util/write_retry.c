/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <unistd.h>
#include <errno.h>

#include "util.h"

ssize_t write_retry(int fd, void *data, size_t size)
{
	ssize_t ret, total = 0;

	while (size > 0) {
		ret = write(fd, data, size);
		if (ret == 0)
			break;
		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return -1;
		}

		data = (char *)data + ret;
		size -= ret;
		total += ret;
	}

	return total;
}
