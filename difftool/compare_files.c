/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * compare_files.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static unsigned char old_buf[MAX_WINDOW_SIZE];
static unsigned char new_buf[MAX_WINDOW_SIZE];

static int open_file(int dirfd, const char *prefix, const char *path)
{
	int fd = openat(dirfd, path, O_RDONLY);

	if (fd < 0) {
		fprintf(stderr, "open %s/%s: %s\n",
			prefix, path, strerror(errno));
	}

	return fd;
}

static int read_blob(const char *prefix, const char *path, bool is_dir,
		     int fd, data_reader_t *rd, file_info_t *fi, void *buffer,
		     off_t offset, size_t size)
{
	ssize_t ret;

	if (is_dir) {
		ret = read_data_at(path, offset, fd, buffer, size);
	} else {
		ret = data_reader_read(rd, fi, offset, buffer, size);
		ret = (ret < 0 || (size_t)ret < size) ? -1 : 0;
	}

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
	int old_fd = -1, new_fd = -1, status = 0, ret;
	uint64_t offset, diff;

	if (old->size != new->size)
		goto out_different;

	if (sd->compare_flags & COMPARE_NO_CONTENTS)
		return 0;

	if (sd->old_is_dir) {
		old_fd = open_file(sd->old_fd, sd->old_path, path);
		if (old_fd < 0)
			goto fail;
	}

	if (sd->new_is_dir) {
		new_fd = open_file(sd->new_fd, sd->new_path, path);
		if (new_fd < 0)
			goto fail;
	}

	for (offset = 0; offset < old->size; offset += diff) {
		diff = old->size - offset;

		if (diff > MAX_WINDOW_SIZE)
			diff = MAX_WINDOW_SIZE;

		ret = read_blob(sd->old_path, path, sd->old_is_dir, old_fd,
				sd->sqfs_old.data, old, old_buf, offset, diff);
		if (ret)
			goto fail;

		ret = read_blob(sd->new_path, path, sd->new_is_dir, new_fd,
				sd->sqfs_new.data, new, new_buf, offset, diff);
		if (ret)
			goto fail;

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
	if (sd->compare_flags & COMPARE_EXTRACT_FILES) {
		if (extract_files(sd, old, new, path))
			goto fail;
	}
	status = 1;
	goto out;
}
