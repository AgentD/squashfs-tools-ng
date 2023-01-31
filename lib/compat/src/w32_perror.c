/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * w32_perror.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void w32_perror(const char *str)
{
	DWORD nStatus = GetLastError();
	LPVOID msg = NULL;

	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		       FORMAT_MESSAGE_FROM_SYSTEM |
		       FORMAT_MESSAGE_IGNORE_INSERTS,
		       NULL, nStatus,
		       MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		       (LPTSTR)&msg, 0, NULL);

	fprintf(stderr, "%s: %s\n", str, (const char *)msg);

	if (msg != NULL)
		LocalFree(msg);
}
#endif
