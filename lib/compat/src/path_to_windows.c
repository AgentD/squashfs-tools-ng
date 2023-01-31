/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * path_to_windows.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#include <stdlib.h>
#include <stdio.h>

#if defined(_WIN32) || defined(__WINDOWS__)
WCHAR *path_to_windows(const char *input)
{
	WCHAR *wpath, *ptr;
	DWORD length;

	length = MultiByteToWideChar(CP_UTF8, 0, input, -1, NULL, 0);
	if (length <= 0) {
		fprintf(stderr, "Converting UTF-8 path to UTF-16: %ld\n",
			GetLastError());
		return NULL;
	}

	wpath = calloc(sizeof(wpath[0]), length + 1);
	if (wpath == NULL) {
		fprintf(stderr,
			"Converting UTF-8 path to UTF-16: out of memory\n");
		return NULL;
	}

	MultiByteToWideChar(CP_UTF8, 0, input, -1, wpath, length + 1);
	wpath[length] = '\0';

	for (ptr = wpath; *ptr != '\0'; ++ptr) {
		if (*ptr == '/')
			*ptr = '\\';
	}

	return wpath;
}
#endif
