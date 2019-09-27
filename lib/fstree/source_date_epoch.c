/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * source_date_epoch.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

sqfs_u32 get_source_date_epoch(void)
{
	const char *str, *ptr;
	sqfs_u32 x, tval = 0;

	str = getenv("SOURCE_DATE_EPOCH");

	if (str == NULL || *str == '\0')
		return 0;

	for (ptr = str; *ptr != '\0'; ++ptr) {
		if (!isdigit(*ptr))
			goto fail_nan;

		x = (*ptr) - '0';

		if (tval > (UINT32_MAX - x) / 10)
			goto fail_ov;

		tval = tval * 10 + x;
	}

	return tval;
fail_ov:
	fprintf(stderr, "WARNING: SOURCE_DATE_EPOCH=%s does not fit into "
		"32 bit integer\n", str);
	return 0;
fail_nan:
	fprintf(stderr, "WARNING: SOURCE_DATE_EPOCH=%s is not a positive "
		"number\n", str);
	return 0;
}
