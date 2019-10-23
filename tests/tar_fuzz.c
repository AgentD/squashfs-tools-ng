/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_fuzz.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/util.h"
#include "tar.h"

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	tar_header_decoded_t hdr;
	int fd, ret;

	if (argc != 2) {
		fputs("usage: tar_fuzz <tarball>\n", stderr);
		return EXIT_FAILURE;
	}

	fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return EXIT_FAILURE;
	}

	for (;;) {
		ret = read_header(fd, &hdr);
		if (ret > 0)
			break;
		if (ret < 0)
			goto fail;

		ret = lseek(fd, hdr.sb.st_size, SEEK_CUR);

		clear_header(&hdr);
		if (ret < 0)
			goto fail;
	}

	close(fd);
	return EXIT_SUCCESS;
fail:
	close(fd);
	return EXIT_FAILURE;
}
