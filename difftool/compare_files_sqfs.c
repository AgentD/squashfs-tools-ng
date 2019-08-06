/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_file_sfqs.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

static unsigned char a_buf[MAX_WINDOW_SIZE];
static unsigned char b_buf[MAX_WINDOW_SIZE];

int compare_files(file_info_t *a, file_info_t *b, const char *path)
{
	uint64_t offset, diff;
	ssize_t ret;

	if (a->size != b->size)
		return 1;

	if (compare_flags & COMPARE_NO_CONTENTS)
		return 0;

	for (offset = 0; offset < a->size; offset += diff) {
		diff = a->size - offset;

		if (diff > MAX_WINDOW_SIZE)
			diff = MAX_WINDOW_SIZE;

		ret = data_reader_read(sqfs_a.data, a, offset, a_buf, diff);
		if (ret < 0 || (size_t)ret < diff) {
			fprintf(stderr, "Failed to read %s from %s\n",
				path, first_path);
			return -1;
		}

		ret = data_reader_read(sqfs_b.data, b, offset, b_buf, diff);
		if (ret < 0 || (size_t)ret < diff) {
			fprintf(stderr, "Failed to read %s from %s\n",
				path, second_path);
			return -1;
		}

		if (memcmp(a_buf, b_buf, diff) != 0)
			return 1;
	}

	return 0;
}
