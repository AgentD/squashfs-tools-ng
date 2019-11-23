/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * getline.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#include <string.h>
#include <stdlib.h>

#ifndef HAVE_GETLINE
ssize_t getline(char **line, size_t *n, FILE *fp)
{
	size_t new_cap, len = 0, cap = 0;
	char *buffer = NULL, *new;
	int c;

	if (feof(fp) || ferror(fp))
		return -1;

	do {
		c = fgetc(fp);

		if (ferror(fp))
			return -1;

		if (c == EOF)
			c = '\n';

		if (len == cap) {
			new_cap = cap ? cap * 2 : 32;
			new = realloc(buffer, new_cap);

			if (new == NULL)
				return -1;

			buffer = new;
			cap = new_cap;
		}

		buffer[len++] = c;
	} while (c != '\n');

	if (len == cap) {
		new = realloc(buffer, cap ? cap * 2 : 32);
		if (new == NULL)
			return -1;

		buffer = new;
	}

	buffer[len] = '\0';

	*line = buffer;
	*n = len;
	return len;
}
#endif
