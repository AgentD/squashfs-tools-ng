/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * filename_sane.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/util.h"

#include <string.h>

bool is_filename_sane(const char *name)
{
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return false;

	while (*name != '\0') {
		if (*name == '/')
			return false;
		++name;
	}

	return true;
}
