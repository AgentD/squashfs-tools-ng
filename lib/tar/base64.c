/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * base64.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "internal.h"

static sqfs_u8 convert(char in)
{
	if (isupper(in))
		return in - 'A';
	if (islower(in))
		return in - 'a' + 26;
	if (isdigit(in))
		return in - '0' + 52;
	if (in == '+')
		return 62;
	if (in == '/' || in == '-')
		return 63;
	return 0;
}

size_t base64_decode(sqfs_u8 *out, const char *in, size_t len)
{
	sqfs_u8 *start = out;

	while (len > 0) {
		unsigned int diff = 0, value = 0;

		while (diff < 4 && len > 0) {
			if (*in == '=' || *in == '_' || *in == '\0') {
				len = 0;
			} else {
				value = (value << 6) | convert(*(in++));
				--len;
				++diff;
			}
		}

		if (diff < 2)
			break;

		value <<= 6 * (4 - diff);

		switch (diff) {
		case 4:  out[2] = value & 0xff; /* fall-through */
		case 3:  out[1] = (value >> 8) & 0xff; /* fall-through */
		default: out[0] = (value >> 16) & 0xff;
		}

		out += (diff * 3) / 4;
	}

	*out = '\0';
	return out - start;
}
