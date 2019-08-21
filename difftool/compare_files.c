/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_files.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static unsigned char old_buf[MAX_WINDOW_SIZE];
static unsigned char new_buf[MAX_WINDOW_SIZE];

int compare_files(file_info_t *old, file_info_t *new, const char *path)
{
	char new_name[strlen(new_path) + strlen(path) + 2];
	char old_name[strlen(old_path) + strlen(path) + 2];
	int old_fd = -1, new_fd = -1, status = 0;
	uint64_t offset, diff;
	ssize_t ret;

	if (old->size != new->size)
		goto out_different;

	if (compare_flags & COMPARE_NO_CONTENTS)
		return 0;

	if (old_is_dir) {
		sprintf(old_name, "%s/%s", old_path, path);

		old_fd = open(old_name, O_RDONLY);
		if (old_fd < 0) {
			perror(old_name);
			goto fail;
		}
	}

	if (new_is_dir) {
		sprintf(new_name, "%s/%s", new_path, path);

		new_fd = open(new_name, O_RDONLY);
		if (new_fd < 0) {
			perror(new_name);
			goto fail;
		}
	}

	for (offset = 0; offset < old->size; offset += diff) {
		diff = old->size - offset;

		if (diff > MAX_WINDOW_SIZE)
			diff = MAX_WINDOW_SIZE;

		if (old_is_dir) {
			if (read_data_at(old_name, offset, old_fd,
					 old_buf, diff))
				goto out;
		} else {
			ret = data_reader_read(sqfs_old.data, old, offset,
					       old_buf, diff);
			if (ret < 0 || (size_t)ret < diff) {
				fprintf(stderr, "Failed to read %s from %s\n",
					path, old_path);
				return -1;
			}
		}

		if (new_is_dir) {
			if (read_data_at(new_name, offset, new_fd,
					 new_buf, diff))
				goto out;
		} else {
			ret = data_reader_read(sqfs_new.data, new, offset,
					       new_buf, diff);
			if (ret < 0 || (size_t)ret < diff) {
				fprintf(stderr, "Failed to read %s from %s\n",
					path, new_path);
				return -1;
			}
		}

		if (memcmp(old_buf, new_buf, diff) != 0)
			goto out_different;
	}
out:
	if (old_fd >= 0)
		close(old_fd);
	if (new_fd >= 0)
		close(new_fd);
	return status;
fail:
	status = -1;
	goto out;
out_different:
	if (compare_flags & COMPARE_EXTRACT_FILES) {
		if (extract_files(old, new, path))
			goto fail;
	}
	status = 1;
	goto out;
}
