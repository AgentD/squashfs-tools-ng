/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * chdir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#ifdef _WIN32
#include <stdlib.h>
#include <stdio.h>

int chdir(const char *path)
{
	WCHAR *wpath;
	int ret;

	wpath = path_to_windows(path);
	if (wpath == NULL)
		return -1;

	if (!SetCurrentDirectoryW(wpath)) {
		fprintf(stderr, "Switching to directory '%s': %ld\n",
			path, GetLastError());
		ret = -1;
	} else {
		ret = 0;
	}

	free(wpath);
	return ret;
}
#endif
