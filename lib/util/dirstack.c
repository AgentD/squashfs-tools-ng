/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dirstack.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>

#include "util.h"

#define STACK_DEPTH 128

static int dirstack[STACK_DEPTH];
static int stacktop = 0;

int pushd(const char *path)
{
	int fd;

	assert(stacktop < STACK_DEPTH);

	fd = open(".", O_DIRECTORY | O_PATH | O_RDONLY | O_CLOEXEC);

	if (fd < 0) {
		perror("open ./");
		return -1;
	}

	if (chdir(path)) {
		perror(path);
		close(fd);
		return -1;
	}

	dirstack[stacktop++] = fd;
	return 0;
}

int pushdn(const char *path, size_t len)
{
	char *temp;
	int ret;

	temp = strndup(path, len);
	if (temp == NULL) {
		perror("pushd");
		return -1;
	}

	ret = pushd(temp);

	free(temp);
	return ret;
}

int popd(void)
{
	int fd;

	assert(stacktop > 0);

	fd = dirstack[stacktop - 1];

	if (fchdir(fd)) {
		perror("popd");
		return -1;
	}

	--stacktop;
	close(fd);
	return 0;
}
