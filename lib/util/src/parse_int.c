/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * alloc.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "util/parse.h"
#include "sqfs/error.h"

#include <ctype.h>

int parse_uint(const char *in, size_t len, size_t *diff,
	       sqfs_u64 vmin, sqfs_u64 vmax, sqfs_u64 *out)
{
	/* init result */
	if (diff != NULL)
		*diff = 0;
	*out = 0;

	/* sequence has at least 1 digit */
	if (len == 0 || !isdigit(*in))
		return SQFS_ERROR_CORRUPTED;

	/* parse sequence */
	while (len > 0 && isdigit(*in)) {
		sqfs_u64 x = *(in++) - '0';
		--len;

		if (diff != NULL)
			++(*diff);

		if ((*out) >= (0xFFFFFFFFFFFFFFFFULL / 10ULL))
			return SQFS_ERROR_OVERFLOW;

		(*out) *= 10;

		if ((*out) > (0xFFFFFFFFFFFFFFFFULL - x))
			return SQFS_ERROR_OVERFLOW;

		(*out) += x;
	}

	/* range check */
	if ((vmin != vmax) && ((*out < vmin) || (*out > vmax)))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	/* if diff is not used, entire  must have been processed */
	if (diff == NULL && (len > 0 && *in != '\0'))
		return SQFS_ERROR_CORRUPTED;

	return 0;
}


int parse_int(const char *in, size_t len, size_t *diff,
	      sqfs_s64 vmin, sqfs_s64 vmax, sqfs_s64 *out)
{
	bool negative = false;
	sqfs_u64 temp;
	int ret;

	if (len > 0 && *in == '-') {
		++in;
		--len;
		negative = true;
	}

	ret = parse_uint(in, len, diff, 0, 0, &temp);
	if (ret)
		return ret;

	if (temp >= 0x7FFFFFFFFFFFFFFFULL)
		return SQFS_ERROR_OVERFLOW;

	if (negative) {
		if (diff != NULL)
			(*diff) += 1;
		*out = -((sqfs_s64)temp);
	} else {
		*out = (sqfs_s64)temp;
	}

	if (vmin != vmax && ((*out < vmin) || (*out > vmax)))
		return SQFS_ERROR_OUT_OF_BOUNDS;

	return 0;
}
