/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <alloca.h>
#include <stdio.h>
#include <errno.h>

#include "util.h"

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
