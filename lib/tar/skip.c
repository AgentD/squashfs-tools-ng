/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include "util.h"
#include "tar.h"

#include <stdio.h>

static int skip_bytes(int fd, uint64_t size)
{
	unsigned char buffer[1024];
	size_t diff;

	while (size != 0) {
		diff = sizeof(buffer);
		if (diff > size)
			diff = size;

		if (read_data("reading tar record padding", fd, buffer, diff))
			return -1;

		size -= diff;
	}

	return 0;
}

int skip_padding(int fd, uint64_t size)
{
	size_t tail = size % 512;

	return tail ? skip_bytes(fd, 512 - tail) : 0;
}

int skip_entry(int fd, uint64_t size)
{
	size_t tail = size % 512;

	return skip_bytes(fd, tail ? (size + 512 - tail) : size);
}
