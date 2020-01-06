/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * parse_size.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"

#include <ctype.h>
#include <limits.h>

int parse_size(const char *what, size_t *out, const char *str,
	       size_t reference)
{
	const char *in = str;
	size_t acc = 0, x;

	if (!isdigit(*in))
		goto fail_nan;

	while (isdigit(*in)) {
		x = *(in++) - '0';

		if (SZ_MUL_OV(acc, 10, &acc))
			goto fail_ov;

		if (SZ_ADD_OV(acc, x, &acc))
			goto fail_ov;
	}

	switch (*in) {
	case 'k':
	case 'K':
		if (acc > ((1UL << (SIZEOF_SIZE_T * CHAR_BIT - 10)) - 1UL))
			goto fail_ov;
		acc <<= 10;
		++in;
		break;
	case 'm':
	case 'M':
		if (acc > ((1UL << (SIZEOF_SIZE_T * CHAR_BIT - 20)) - 1UL))
			goto fail_ov;
		acc <<= 20;
		++in;
		break;
	case 'g':
	case 'G':
		if (acc > ((1UL << (SIZEOF_SIZE_T * CHAR_BIT - 30)) - 1UL))
			goto fail_ov;
		acc <<= 30;
		++in;
		break;
	case '%':
		if (reference == 0)
			goto fail_suffix;

		if (SZ_MUL_OV(acc, reference, &acc))
			goto fail_ov;

		acc /= 100;
		break;
	case '\0':
		break;
	default:
		goto fail_suffix;
	}

	if (*in != '\0')
		goto fail_suffix;

	*out = acc;
	return 0;
fail_nan:
	fprintf(stderr, "%s: '%s' is not a number.\n", what, str);
	return -1;
fail_ov:
	fprintf(stderr, "%s: numeric overflow parsing '%s'.\n", what, str);
	return -1;
fail_suffix:
	fprintf(stderr, "%s: unknown suffix in '%s'.\n", what, str);
	return -1;
}
