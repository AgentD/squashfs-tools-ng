/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * print_size.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "common.h"

void print_size(sqfs_u64 size, char *buffer, bool round_to_int)
{
	static const char *fractions = "0112334456678899";
	static const char *suffices = "kMGTPEZY";
	unsigned int fraction;
	int suffix = -1;

	while (size > 1024) {
		++suffix;
		fraction = size % 1024;
		size /= 1024;
	}

	if (suffix >= 0) {
		fraction /= 64;

		if (round_to_int) {
			size = fraction >= 8 ? (size + 1) : size;

			sprintf(buffer, "%u%c", (unsigned int)size,
				suffices[suffix]);
		} else {
			sprintf(buffer, "%u.%c%c", (unsigned int)size,
				fractions[fraction], suffices[suffix]);
		}
	} else {
		sprintf(buffer, "%u", (unsigned int)size);
	}
}
