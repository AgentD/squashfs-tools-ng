/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <string.h>

#include "util.h"

int canonicalize_name(char *filename)
{
	char *ptr = filename;
	int i;

	while (*ptr == '/')
		++ptr;

	if (ptr != filename) {
		memmove(filename, ptr, strlen(ptr) + 1);
		ptr = filename;
	}

	while (*ptr != '\0') {
		if (*ptr == '/') {
			for (i = 0; ptr[i] == '/'; ++i)
				;

			if (i > 1)
				memmove(ptr + 1, ptr + i, strlen(ptr + i) + 1);
		}

		if (ptr[0] == '/' && ptr[1] == '\0') {
			*ptr = '\0';
			break;
		}

		++ptr;
	}

	ptr = filename;

	while (*ptr != '\0') {
		if (ptr[0] == '.') {
			if (ptr[1] == '/' || ptr[1] == '\0')
				return -1;

			if (ptr[1] == '.' &&
			    (ptr[2] == '/' || ptr[2] == '\0')) {
				return -1;
			}
		}

		while (*ptr != '\0' && *ptr != '/')
			++ptr;
		if (*ptr == '/')
			++ptr;
	}

	return 0;
}
