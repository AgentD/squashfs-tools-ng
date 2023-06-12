/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar_fuzz.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "io/file.h"
#include "tar/tar.h"

#include <stdlib.h>
#include <stdio.h>

int main(int argc, char **argv)
{
	tar_header_decoded_t hdr;
	sqfs_istream_t *fp;
	int ret;

	if (argc != 2) {
		fputs("usage: tar_fuzz <tarball>\n", stderr);
		return EXIT_FAILURE;
	}

	fp = istream_open_file(argv[1]);
	if (fp == NULL)
		return EXIT_FAILURE;

	for (;;) {
		ret = read_header(fp, &hdr);
		if (ret > 0)
			break;
		if (ret < 0)
			goto fail;

		ret = istream_skip(fp, hdr.record_size);

		clear_header(&hdr);
		if (ret < 0)
			goto fail;
	}

	sqfs_drop(fp);
	return EXIT_SUCCESS;
fail:
	sqfs_drop(fp);
	return EXIT_FAILURE;
}
