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

	/* open first source */
	sd.old_fd = open(sd.old_path, O_DIRECTORY | O_PATH | O_RDONLY);
	if (sd.old_fd < 0) {
		if (errno == ENOTDIR) {
			sd.old_is_dir = false;

			if (sqfs_reader_open(&sd.sqfs_old, sd.old_path, 0))
				return 2;
		} else {
			perror(sd.old_path);
			return 2;
		}
	} else {
		sd.old_is_dir = true;
		if (fstree_init(&sd.sqfs_old.fs, 512, NULL))
			return 2;

		if (fstree_from_dir(&sd.sqfs_old.fs, sd.old_path, 0)) {
			fstree_cleanup(&sd.sqfs_old.fs);
			return 2;
		}

		tree_node_sort_recursive(sd.sqfs_old.fs.root);
	}

	/* open second source */
	sd.new_fd = open(sd.new_path, O_DIRECTORY | O_PATH | O_RDONLY);
	if (sd.new_fd < 0) {
		if (errno == ENOTDIR) {
			sd.new_is_dir = false;

			if (sqfs_reader_open(&sd.sqfs_new, sd.new_path, 0)) {
				status = 2;
				goto out_sqfs_old;
			}
		} else {
			status = 2;
			perror(sd.new_path);
			goto out_sqfs_old;
		}
	} else {
		sd.new_is_dir = true;
		if (fstree_init(&sd.sqfs_new.fs, 512, NULL)) {
			status = 2;
			goto out_sqfs_old;
		}

		if (fstree_from_dir(&sd.sqfs_new.fs, sd.new_path, 0)) {
			status = 2;
			fstree_cleanup(&sd.sqfs_old.fs);
			goto out_sqfs_old;
		}

		tree_node_sort_recursive(sd.sqfs_new.fs.root);
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

	if (sd.compare_super && !sd.old_is_dir && !sd.new_is_dir) {
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
	if (sd.new_is_dir) {
		close(sd.new_fd);
		fstree_cleanup(&sd.sqfs_new.fs);
	} else {
		sqfs_reader_close(&sd.sqfs_new);
	}
out_sqfs_old:
	if (sd.old_is_dir) {
		close(sd.old_fd);
		fstree_cleanup(&sd.sqfs_old.fs);
	} else {
		sqfs_reader_close(&sd.sqfs_old);
	}
	return status;
}
