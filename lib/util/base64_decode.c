/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * base64_decode.c
 *
 * Copyright (C) 2022 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/util.h"
#include "util/test.h"

#include <ctype.h>

static int base64_digit(int c)
{
	if (isupper(c))
		return c - 'A';
	if (islower(c))
		return c - 'a' + 26;
	if (isdigit(c))
		return c - '0' + 52;
	if (c == '+')
		return 62;
	if (c == '/' || c == '-')
		return 63;
	return -1;
}

int base64_decode(const char *in, size_t in_len, sqfs_u8 *out, size_t *out_len)
{
	int i1, i2, i3, i4;
	size_t count = 0;

	while (in_len >= 4) {
		i1 = base64_digit(*(in++));
		i2 = base64_digit(*(in++));
		i3 = *(in++);
		i4 = *(in++);
		in_len -= 4;

		if (i1 < 0 || i2 < 0 || count >= *out_len)
			goto fail;

		out[count++] = (i1 << 2) | (i2 >> 4);

		if (i3 == '=' || i3 == '_') {
			if ((i4 != '=' && i4 != '_') || in_len > 0)
				goto fail;
			break;
		}

		i3 = base64_digit(i3);
		if (i3 < 0 || count >= *out_len)
			goto fail;

		out[count++] = ((i2 & 0x0F) << 4) | (i3 >> 2);

		if (i4 == '=' || i4 == '_') {
			if (in_len > 0)
				goto fail;
			break;
		}

		i4 = base64_digit(i4);
		if (i4 < 0 || count >= *out_len)
			goto fail;

		out[count++] = ((i3 & 0x3) << 6) | i4;
	}

	/* libarchive has this bizarre bastardization of truncated base64 */
	if (in_len > 0) {
		if (in_len == 1)
			goto fail;

		i1 = base64_digit(*(in++));
		i2 = base64_digit(*(in++));
		in_len -= 2;

		if (i1 < 0 || i2 < 0 || count >= *out_len)
			goto fail;

		out[count++] = (i1 << 2) | (i2 >> 4);

		if (in_len > 0) {
			i3 = *(in++);
			--in_len;

			if (i3 != '=' && i3 != '_') {
				i3 = base64_digit(i3);
				if (i3 < 0 || count >= *out_len)
					goto fail;

				out[count++] = ((i2 & 0x0F) << 4) | (i3 >> 2);
			}
		}
	}

	*out_len = count;
	return 0;
fail:
	*out_len = 0;
	return -1;
}
