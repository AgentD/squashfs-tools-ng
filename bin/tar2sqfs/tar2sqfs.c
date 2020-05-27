/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * tar2sqfs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "tar2sqfs.h"

int main(int argc, char **argv)
{
	int status = EXIT_FAILURE;
	FILE *input_file = NULL;
	sqfs_writer_t sqfs;

	process_args(argc, argv);

#ifdef _WIN32
	_setmode(_fileno(stdin), _O_BINARY);
	input_file = stdin;
#else
	input_file = freopen(NULL, "rb", stdin);
#endif

	if (input_file == NULL) {
		perror("changing stdin to binary mode");
		return EXIT_FAILURE;
	}

	memset(&sqfs, 0, sizeof(sqfs));
	if (sqfs_writer_init(&sqfs, &cfg))
		return EXIT_FAILURE;

	if (process_tarball(input_file, &sqfs))
		goto out;

	if (fstree_post_process(&sqfs.fs))
		goto out;

	if (sqfs_writer_finish(&sqfs, &cfg))
		goto out;

	status = EXIT_SUCCESS;
out:
	sqfs_writer_cleanup(&sqfs, status);
	return status;
}
