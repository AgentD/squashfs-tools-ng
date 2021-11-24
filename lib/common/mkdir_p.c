/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * mkdir_p.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
/*
  Supported paths:
   - <letter>:\ <absolute path>
   - \\<server>\<share>\ <absolute path>
   - \\?\<letter>:\ <absolute path>
   - \\?\UNC\<server>\<share>\ <absolute path>
   - Relative path not starting with '\'
 */

static WCHAR *skip_unc_path(WCHAR *ptr)
{
	/* server */
	if (*ptr == '\0' || *ptr == '\\')
		return NULL;

	while (*ptr != '\0' && *ptr != '\\')
		++ptr;

	if (*(ptr++) != '\\')
		return NULL;

	/* share */
	if (*ptr == '\0' || *ptr == '\\')
		return NULL;

	while (*ptr != '\0' && *ptr != '\\')
		++ptr;

	return (*ptr == '\\') ? (ptr + 1) : ptr;
}

static WCHAR *skip_prefix(WCHAR *ptr)
{
	if (isalpha(ptr[0]) && ptr[1] == ':' && ptr[2] == '\\')
		return ptr + 3;

	if (ptr[0] == '\\' && ptr[1] == '\\') {
		if (ptr[2] == '?') {
			if (ptr[3] != '\\')
				return NULL;

			ptr += 4;

			if ((ptr[0] == 'u' || ptr[0] == 'U') &&
			    (ptr[1] == 'n' || ptr[1] == 'N') &&
			    (ptr[2] == 'c' || ptr[2] == 'C') &&
			    ptr[3] == '\\') {
				ptr += 4;

				return skip_unc_path(ptr);
			}

			if (isalpha(ptr[0]) && ptr[1] == ':' && ptr[2] == '\\')
				return ptr + 3;

			return NULL;
		}

		return skip_unc_path(ptr);
	}

	if (ptr[0] == '\\')
		return NULL;

	return ptr;
}

int mkdir_p(const char *path)
{
	WCHAR *wpath, *ptr, *end;
	DWORD error;
	bool done;


	wpath = path_to_windows(path);
	if (wpath == NULL)
		return -1;

	ptr = skip_prefix(wpath);
	if (ptr == NULL) {
		fprintf(stderr, "Illegal or unsupported path: %s\n", path);
		goto fail;
	}

	while (*ptr != '\0') {
		if (*ptr == '\\') {
			++ptr;
			continue;
		}

		for (end = ptr; *end != '\0' && *end != '\\'; ++end)
			++end;

		if (*end == '\\') {
			done = false;
			*end = '\0';
		} else {
			done = true;
		}

		if (!CreateDirectoryW(wpath, NULL)) {
			error = GetLastError();

			if (error != ERROR_ALREADY_EXISTS) {
				fprintf(stderr, "Creating %s: %ld\n",
					path, error);
				goto fail;
			}
		}

		if (!done)
			*end = '\\';

		ptr = done ? end : (end + 1);
	}

	free(wpath);
	return 0;
fail:
	free(wpath);
	return -1;
}
#else
int mkdir_p(const char *path)
{
	size_t i, len;
	char *buffer;

	while (path[0] == '/' && path[1] == '/')
		++path;

	if (*path == '\0' || (path[0] == '/' && path[1] == '\0'))
		return 0;

	len = strlen(path) + 1;
	buffer = alloca(len);

	for (i = 0; i < len; ++i) {
		if (i > 0 && (path[i] == '/' || path[i] == '\0')) {
			buffer[i] = '\0';

			if (mkdir(buffer, 0755) != 0) {
				if (errno != EEXIST) {
					fprintf(stderr, "mkdir %s: %s\n",
						buffer, strerror(errno));
					return -1;
				}
			}
		}

		buffer[i] = path[i];
	}

	return 0;
}
#endif
