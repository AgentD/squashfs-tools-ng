/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filename_sane.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <string.h>

#if defined(_WIN32) || defined(__WINDOWS__) || defined(TEST_WIN32)
#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

static const char *bad_names[] = {
	"CON", "PRN", "AUX", "NUL",
	"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
	"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
};

static bool is_allowed_by_os(const char *name)
{
	size_t len, i;

	for (i = 0; i < sizeof(bad_names) / sizeof(bad_names[0]); ++i) {
		len = strlen(bad_names[i]);

		if (strncasecmp(name, bad_names[i], len) != 0)
			continue;

		if (name[len] == '\0')
			return false;

		if (name[len] == '.' && strchr(name + len + 1, '.') == NULL)
			return false;
	}

	return true;
}
#else
static bool is_allowed_by_os(const char *name)
{
	(void)name;
	return true;
}
#endif

bool is_filename_sane(const char *name, bool check_os_specific)
{
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return false;

	if (check_os_specific && !is_allowed_by_os(name))
		return false;

	while (*name != '\0') {
		if (*name == '/')
			return false;

#if defined(_WIN32) || defined(__WINDOWS__) || defined(TEST_WIN32)
		if (check_os_specific) {
			if (*name == '<' || *name == '>' || *name == ':')
				return false;
			if (*name == '"' || *name == '|' || *name == '?')
				return false;
			if (*name == '*' || *name == '\\' || *name <= 31)
				return false;
		}
#endif

		++name;
	}

	return true;
}
