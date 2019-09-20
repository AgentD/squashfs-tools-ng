/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_files.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static unsigned char old_buf[MAX_WINDOW_SIZE];
static unsigned char new_buf[MAX_WINDOW_SIZE];

static int read_blob(const char *prefix, const char *path,
		     data_reader_t *rd, file_info_t *fi, void *buffer,
		     off_t offset, size_t size)
{
	ssize_t ret;

	ret = data_reader_read(rd, fi, offset, buffer, size);
	ret = (ret < 0 || (size_t)ret < size) ? -1 : 0;

	if (ret) {
		fprintf(stderr, "Failed to read %s from %s\n",
			path, prefix);
		return -1;
	}

	return 0;
}

int compare_files(sqfsdiff_t *sd, file_info_t *old, file_info_t *new,
		  const char *path)
{
	uint64_t offset, diff;
	int status = 0, ret;

	if (old->size != new->size)
		goto out_different;

	if (sd->compare_flags & COMPARE_NO_CONTENTS)
		return 0;

	for (offset = 0; offset < old->size; offset += diff) {
		diff = old->size - offset;

		if (diff > MAX_WINDOW_SIZE)
			diff = MAX_WINDOW_SIZE;

		ret = read_blob(sd->old_path, path,
				sd->sqfs_old.data, old, old_buf, offset, diff);
		if (ret)
			return -1;

		ret = read_blob(sd->new_path, path,
				sd->sqfs_new.data, new, new_buf, offset, diff);
		if (ret)
			return -1;

		if (memcmp(old_buf, new_buf, diff) != 0)
			goto out_different;
	}

	return status;
out_different:
	if (sd->compare_flags & COMPARE_EXTRACT_FILES) {
		if (extract_files(sd, old, new, path))
			return -1;
	}
	return 1;
}
