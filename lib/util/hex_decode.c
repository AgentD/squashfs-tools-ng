/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * hex_decode.h
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "util/util.h"

#include <ctype.h>

static sqfs_u8 xdigit(int in)
{
	if (isupper(in))
		return in - 'A' + 10;
	if (islower(in))
		return in - 'a' + 10;
	return in - '0';
}

int hex_decode(const char *in, size_t in_sz, sqfs_u8 *out, size_t out_sz)
{
	while (out_sz > 0 && in_sz >= 2 &&
	       isxdigit(in[0]) && isxdigit(in[1])) {
		sqfs_u8 hi = xdigit(*(in++));
		sqfs_u8 lo = xdigit(*(in++));

		*(out++) = (hi << 4) | lo;

		in_sz -= 2;
		--out_sz;
	}

	return (in_sz > 0) ? -1 : 0;
}
