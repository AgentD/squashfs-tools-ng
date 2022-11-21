/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * strchrnul.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "compat.h"

#ifndef HAVE_STRCHRNUL
char *strchrnul(const char *s, int c)
{
	while (*s && *((unsigned char *)s) != c)
		++s;

	return (char *)s;
}
#endif
