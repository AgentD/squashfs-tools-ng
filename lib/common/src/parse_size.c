/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * parse_size.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "common.h"
#include "util/parse.h"

#include <ctype.h>
#include <limits.h>

int parse_size(const char *what, size_t *out, const char *str,
	       size_t reference)
{
	sqfs_u64 val;
	size_t diff;
	int ret;

	ret = parse_uint(str, -1, &diff, 0, SIZE_MAX, &val);
	if (ret == SQFS_ERROR_OVERFLOW)
		goto fail_ov;
	if (ret != 0)
		goto fail_nan;

	*out = val;

	switch (str[diff]) {
	case 'k':
	case 'K':
		if (SZ_MUL_OV(*out, 1024, out))
			goto fail_ov;
		++diff;
		break;
	case 'm':
	case 'M':
		if (SZ_MUL_OV(*out, 1048576, out))
			goto fail_ov;
		++diff;
		break;
	case 'g':
	case 'G':
		if (SZ_MUL_OV(*out, 1073741824, out))
			goto fail_ov;
		++diff;
		break;
	case '%':
		if (reference == 0)
			goto fail_suffix;

		if (SZ_MUL_OV(*out, reference, out))
			goto fail_ov;

		*out /= 100;
		break;
	case '\0':
		break;
	default:
		goto fail_suffix;
	}

	if (str[diff] != '\0')
		goto fail_suffix;

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
