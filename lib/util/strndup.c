/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * strndup.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/compat.h"

#include <string.h>
#include <stdlib.h>

#ifndef HAVE_STRNDUP
char *strndup(const char *str, size_t max_len)
{
	size_t len = 0;
	char *out;

	while (len < max_len && str[len] != '\0')
		++len;

	out = malloc(len + 1);

	if (out != NULL) {
		memcpy(out, str, len);
		out[len] = '\0';
	}

	return out;
}
#endif
