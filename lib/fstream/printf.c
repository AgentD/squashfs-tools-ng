/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * printf.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

int ostream_printf(ostream_t *strm, const char *fmt, ...)
{
	char *temp = NULL;
	va_list ap;
	int ret;

	va_start(ap, fmt);

	ret = vasprintf(&temp, fmt, ap);
	if (ret < 0)
		perror(strm->get_filename(strm));
	va_end(ap);

	if (ret < 0)
		return -1;

	if (strm->append(strm, temp, ret))
		ret = -1;

	free(temp);
	return ret;
}
