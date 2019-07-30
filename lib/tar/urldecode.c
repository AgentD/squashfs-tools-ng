/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * urldecode.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

static int xdigit(int x)
{
	if (isupper(x))
		return x - 'A' + 0x0A;
	if (islower(x))
		return x - 'a' + 0x0A;
	return x - '0';
}

void urldecode(char *str)
{
	unsigned char *out = (unsigned char *)str;
	char *in = str;
	int x;

	while (*in != '\0') {
		x = *(in++);

		if (x == '%' && isxdigit(in[0]) && isxdigit(in[1])) {
			x = xdigit(*(in++)) << 4;
			x |= xdigit(*(in++));
		}

		*(out++) = x;
	}

	*out = '\0';
}
