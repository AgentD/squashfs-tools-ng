/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * getsubopt.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/compat.h"

#include <stdlib.h>
#include <string.h>

#ifndef HAVE_GETSUBOPT
int getsubopt(char **opt, char *const *keys, char **val)
{
	char *str = *opt;
	size_t i, len;

	*val = NULL;
	*opt = strchr(str, ',');

	if (*opt == NULL) {
		*opt = str + strlen(str);
	} else {
		*(*opt)++ = '\0';
	}

	for (i = 0; keys[i]; ++i) {
		len = strlen(keys[i]);

		if (strncmp(keys[i], str, len) != 0)
			continue;

		if (str[len] != '=' && str[len] != '\0')
			continue;

		if (str[len] == '=')
			*val = str + len + 1;

		return i;
	}

	return -1;
}
#endif
