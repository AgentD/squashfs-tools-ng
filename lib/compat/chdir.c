/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * chdir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#include "util/util.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

int chdir(const char *path)
{
	WCHAR *wpath, *ptr;
	DWORD length;
	int ret;

	length = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
	if (length <= 0) {
		fprintf(stderr, "Converting '%s' to UTF-16: %ld\n",
			path, GetLastError());
		return -1;
	}

	wpath = alloc_array(sizeof(wpath[0]), length + 1);
	if (wpath == NULL) {
		fprintf(stderr, "Converting '%s' to UTF-16: out of memory\n",
			path);
		return -1;
	}

	MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, length + 1);
	wpath[length] = '\0';

	for (ptr = wpath; *ptr != '\0'; ++ptr) {
		if (*ptr == '/')
			*ptr = '\\';
	}

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
