/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_files.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

static unsigned char a_buf[MAX_WINDOW_SIZE];
static unsigned char b_buf[MAX_WINDOW_SIZE];

int compare_files(file_info_t *a, file_info_t *b, const char *path)
{
	char second_name[strlen(second_path) + strlen(path) + 2];
	char first_name[strlen(first_path) + strlen(path) + 2];
	int afd = -1, bfd = -1, status = 0;
	uint64_t offset, diff;
	ssize_t ret;

	if (a->size != b->size)
		goto out_different;

	if (compare_flags & COMPARE_NO_CONTENTS)
		return 0;

	if (a_is_dir) {
		sprintf(first_name, "%s/%s", first_path, path);

		afd = open(first_name, O_RDONLY);
		if (afd < 0) {
			perror(first_name);
			goto fail;
		}
	}

	if (b_is_dir) {
		sprintf(second_name, "%s/%s", second_path, path);

		bfd = open(second_name, O_RDONLY);
		if (bfd < 0) {
			perror(second_name);
			goto fail;
		}
	}

	for (offset = 0; offset < a->size; offset += diff) {
		diff = a->size - offset;

		if (diff > MAX_WINDOW_SIZE)
			diff = MAX_WINDOW_SIZE;

		if (a_is_dir) {
			if (read_data_at(first_name, offset, afd, a_buf, diff))
				goto out;
		} else {
			ret = data_reader_read(sqfs_a.data, a, offset,
					       a_buf, diff);
			if (ret < 0 || (size_t)ret < diff) {
				fprintf(stderr, "Failed to read %s from %s\n",
					path, first_path);
				return -1;
			}
		}

		if (b_is_dir) {
			if (read_data_at(second_name, offset, bfd, b_buf, diff))
				goto out;
		} else {
			ret = data_reader_read(sqfs_b.data, b, offset,
					       b_buf, diff);
			if (ret < 0 || (size_t)ret < diff) {
				fprintf(stderr, "Failed to read %s from %s\n",
					path, second_path);
				return -1;
			}
		}

		if (memcmp(a_buf, b_buf, diff) != 0)
			goto out_different;
	}
out:
	if (afd >= 0)
		close(afd);
	if (bfd >= 0)
		close(bfd);
	return status;
fail:
	status = -1;
	goto out;
out_different:
	if (compare_flags & COMPARE_EXTRACT_FILES) {
		if (extract_files(a, b, path))
			goto fail;
	}
	status = 1;
	goto out;
}
