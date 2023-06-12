/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * extract.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static int extract(sqfs_data_reader_t *data, const sqfs_inode_generic_t *inode,
		   const char *prefix, const char *path, size_t block_size)
{
	char *ptr, *temp;
	sqfs_ostream_t *fp;

	temp = alloca(strlen(prefix) + strlen(path) + 2);
	sprintf(temp, "%s/%s", prefix, path);

	ptr = strrchr(temp, '/');
	*ptr = '\0';
	if (mkdir_p(temp))
		return -1;
	*ptr = '/';

	fp = ostream_open_file(temp, SQFS_FILE_OPEN_OVERWRITE);
	if (fp == NULL) {
		perror(temp);
		return -1;
	}

	if (sqfs_data_reader_dump(path, data, inode, fp, block_size)) {
		sqfs_drop(fp);
		return -1;
	}

	fp->flush(fp);
	sqfs_drop(fp);
	return 0;
}

int extract_files(sqfsdiff_t *sd, const sqfs_inode_generic_t *old,
		  const sqfs_inode_generic_t *new,
		  const char *path)
{
	if (old != NULL) {
		if (extract(sd->sqfs_old.data, old, "old",
			    path, sd->sqfs_old.super.block_size))
			return -1;
	}

	if (new != NULL) {
		if (extract(sd->sqfs_new.data, new, "new",
			    path, sd->sqfs_new.super.block_size))
			return -1;
	}

	return 0;
}
