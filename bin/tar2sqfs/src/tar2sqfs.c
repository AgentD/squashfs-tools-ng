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
	sqfs_istream_t *input_file = NULL;
	dir_iterator_t *tar = NULL;
	sqfs_writer_t sqfs;

	process_args(argc, argv);

	input_file = istream_open_stdin();
	if (input_file == NULL)
		return EXIT_FAILURE;

	tar = tar_open_stream(input_file);
	sqfs_drop(input_file);
	if (tar == NULL) {
		fputs("Creating tar stream: out-of-memory\n", stderr);
		return EXIT_FAILURE;
	}

	memset(&sqfs, 0, sizeof(sqfs));
	if (sqfs_writer_init(&sqfs, &cfg))
		goto out_it;

	if (process_tarball(tar, &sqfs))
		goto out;

	if (fstree_post_process(&sqfs.fs))
		goto out;

	if (sqfs_writer_finish(&sqfs, &cfg))
		goto out;

	status = EXIT_SUCCESS;
out:
	sqfs_writer_cleanup(&sqfs, status);
out_it:
	sqfs_drop(tar);
	return status;
}
