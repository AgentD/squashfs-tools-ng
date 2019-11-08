/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filename_sane.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <string.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

static const char *bad_names[] = {
	".",
	"..",
#ifdef _WIN32
	"CON", "PRN", "AUX", "NUL",
	"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
	"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
#endif
};

bool is_filename_sane(const char *name)
{
	size_t i = 0;

	for (i = 0; i < sizeof(bad_names) / sizeof(bad_names[0]); ++i) {
#ifdef _WIN32
		size_t len = strlen(bad_names[i]);

		if (strncasecmp(name, bad_names[i], len) != 0)
			continue;

		if (name[len] == '\0')
			return false;

		if (name[len] == '.' && strchr(name + len + 1, '.') == NULL)
			return false;
#else
		if (strcmp(name, bad_names[i]) == 0)
			return false;
#endif
	}

	while (*name != '\0') {
		if (*name == '/' || *name == '\\')
			return false;

#ifdef _WIN32
		if (*name == '<' || *name == '>' || *name == ':')
			return false;
		if (*name == '"' || *name == '|' || *name == '?')
			return false;
		if (*name == '*' || *name <= 31)
			return false;
#endif

		++name;
	}

	return true;
}
