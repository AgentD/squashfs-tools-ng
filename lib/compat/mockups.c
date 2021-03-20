/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mockups.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#include <stdio.h>

#ifdef _WIN32
int fchownat(int dirfd, const char *path, int uid, int gid, int flags)
{
	if (dirfd != AT_FDCWD) {
		fputs("[FIXME] fchownat stub only supports AT_FDCWD!\n",
		      stderr);
		return -1;
	}

	if (flags != 0 && flags != AT_SYMLINK_NOFOLLOW) {
		fputs("[FIXME] fchownat stub used with an unknown flag!\n",
		      stderr);
		return -1;
	}

	(void)path;
	(void)uid;
	(void)gid;
	return 0;
}

int fchmodat(int dirfd, const char *path, int mode, int flags)
{
	if (dirfd != AT_FDCWD) {
		fputs("[FIXME] fchmodat stub only supports AT_FDCWD!\n",
		      stderr);
		return -1;
	}

	if (flags != 0 && flags != AT_SYMLINK_NOFOLLOW) {
		fputs("[FIXME] fchmodat stub used with an unknown flag!\n",
		      stderr);
		return -1;
	}

	(void)path;
	(void)mode;
	return 0;
}
#endif
