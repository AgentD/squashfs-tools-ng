/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * extract.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "difftool.h"

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

int extract_files(file_info_t *a, file_info_t *b, const char *path)
{
	if (a != NULL && !a_is_dir) {
		if (extract(sqfs_a.data, a, path, 'a'))
			return -1;
	}

	if (b != NULL && !b_is_dir) {
		if (extract(sqfs_b.data, b, path, 'b'))
			return -1;
	}
	return 0;
}
