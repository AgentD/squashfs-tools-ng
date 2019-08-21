/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * extract.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "sqfsdiff.h"

static int extract(data_reader_t *data, file_info_t *fi,
		   const char *path, char prefix)
{
	char *ptr, *temp = alloca(strlen(path) + 3);
	int fd;

	temp[0] = prefix;
	temp[1] = '/';
	strcpy(temp + 2, path);

	ptr = strrchr(temp, '/');
	*ptr = '\0';
	if (mkdir_p(temp))
		return -1;
	*ptr = '/';

	fd = open(temp, O_CREAT | O_EXCL | O_WRONLY, 0600);
	if (fd < 0) {
		perror(temp);
		return -1;
	}

	if (data_reader_dump_file(data, fi, fd, true)) {
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}

int extract_files(file_info_t *old, file_info_t *new, const char *path)
{
	if (old != NULL && !old_is_dir) {
		if (extract(sqfs_old.data, old, path, 'a'))
			return -1;
	}

	if (new != NULL && !new_is_dir) {
		if (extract(sqfs_new.data, new, path, 'b'))
			return -1;
	}
	return 0;
}
