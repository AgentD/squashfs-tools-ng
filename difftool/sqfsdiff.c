/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * sqfsdiff.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

int main(int argc, char **argv)
{
	int status, ret = 0;
	sqfsdiff_t sd;

	memset(&sd, 0, sizeof(sd));
	process_options(&sd, argc, argv);

	if (sd.extract_dir != NULL) {
		if (mkdir_p(sd.extract_dir))
			return 2;
	}

	if (sqfs_reader_open(&sd.sqfs_old, sd.old_path))
		return 2;

	if (sqfs_reader_open(&sd.sqfs_new, sd.new_path)) {
		status = 2;
		goto out_sqfs_old;
	}

	if (sd.extract_dir != NULL) {
		if (chdir(sd.extract_dir)) {
			perror(sd.extract_dir);
			ret = -1;
			goto out;
		}
	}

	ret = node_compare(&sd, sd.sqfs_old.fs.root, sd.sqfs_new.fs.root);
	if (ret != 0)
		goto out;

	if (sd.compare_super) {
		ret = compare_super_blocks(&sd.sqfs_old.super,
					   &sd.sqfs_new.super);
		if (ret != 0)
			goto out;
	}
out:
	if (ret < 0) {
		status = 2;
	} else if (ret > 0) {
		status = 1;
	} else {
		status = 0;
	}
	sqfs_reader_close(&sd.sqfs_new);
out_sqfs_old:
	sqfs_reader_close(&sd.sqfs_old);
	return status;
}
