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
		     sqfs_data_reader_t *rd, const sqfs_inode_generic_t *inode,
		     void *buffer, sqfs_u64 offset, size_t size)
{
	sqfs_s32 ret;

	ret = sqfs_data_reader_read(rd, inode, offset, buffer, size);
	ret = (ret < 0 || (size_t)ret < size) ? -1 : 0;

	if (ret) {
		fprintf(stderr, "Failed to read %s from %s\n",
			path, prefix);
		return -1;
	}

	return 0;
}

int compare_files(sqfsdiff_t *sd, const sqfs_inode_generic_t *old,
		  const sqfs_inode_generic_t *new, const char *path)
{
	sqfs_u64 offset, diff, oldsz, newsz;
	int status = 0, ret;

	sqfs_inode_get_file_size(old, &oldsz);
	sqfs_inode_get_file_size(new, &newsz);

	if (oldsz != newsz)
		goto out_different;

	if (sd->compare_flags & COMPARE_NO_CONTENTS)
		return 0;

	for (offset = 0; offset < oldsz; offset += diff) {
		diff = oldsz - offset;

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
