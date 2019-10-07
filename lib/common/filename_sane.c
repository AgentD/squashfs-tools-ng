/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filename_sane.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

bool is_filename_sane(const char *name)
{
	if (name[0] == '.') {
		if (name[1] == '\0')
			return false;

		if (name[1] == '.' && name[2] == '\0')
			return false;
	}

	while (*name != '\0') {
		if (*name == '/')
			return false;
		++name;
	}

	return true;
}
