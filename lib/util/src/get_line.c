/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * get_line.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "util/parse.h"
#include "sqfs/io.h"
#include "sqfs/error.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void ltrim(char *buffer)
{
	size_t i = 0;

	while (isspace(buffer[i]))
		++i;

	if (i > 0)
		memmove(buffer, buffer + i, strlen(buffer + i) + 1);
}

void rtrim(char *buffer)
{
	size_t i = strlen(buffer);

	while (i > 0 && isspace(buffer[i - 1]))
		--i;

	buffer[i] = '\0';
}

void trim(char *buffer)
{
	ltrim(buffer);
	rtrim(buffer);
}

static size_t trim_flags(char *buffer, int flags)
{
	if (flags & ISTREAM_LINE_LTRIM)
		ltrim(buffer);

	if (flags & ISTREAM_LINE_RTRIM)
		rtrim(buffer);

	return strlen(buffer);
}

int istream_get_line(sqfs_istream_t *strm, char **out,
		     size_t *line_num, int flags)
{
	char *line = NULL, *new;
	size_t line_len = 0;
	int err;

	for (;;) {
		bool have_line = false;
		size_t i, count, avail;
		const sqfs_u8 *ptr;

		err = strm->get_buffered_data(strm, &ptr, &avail, 0);
		if (err < 0)
			goto fail;
		if (err > 0) {
			if (line_len == 0)
				goto out_eof;

			line_len = trim_flags(line, flags);
			if (line_len > 0 ||!(flags & ISTREAM_LINE_SKIP_EMPTY))
				break;

			goto out_eof;
		}

		for (i = 0; i < avail; ++i) {
			if (ptr[i] == '\n')
				break;
		}

		if (i < avail) {
			count = i++;
			have_line = true;
		} else {
			count = i;
		}

		new = realloc(line, line_len + count + 1);
		if (new == NULL) {
			err = SQFS_ERROR_ALLOC;
			goto fail;
		}

		line = new;
		memcpy(line + line_len, ptr, count);
		line_len += count;
		line[line_len] = '\0';

		strm->advance_buffer(strm, i);

		if (have_line) {
			if (line_len > 0 && line[line_len - 1] == '\r')
				line[--line_len] = '\0';

			line_len = trim_flags(line, flags);
			if (line_len > 0 || !(flags & ISTREAM_LINE_SKIP_EMPTY))
				break;

			free(line);
			line = NULL;
			*line_num += 1;
		}
	}

	new = realloc(line, line_len + 1);
	if (new != NULL)
		line = new;

	*out = line;
	return 0;
fail: {
	os_error_t temp = get_os_error_state();
	free(line);
	*out = NULL;
	set_os_error_state(temp);
	return err;
}
out_eof:
	free(line);
	*out = NULL;
	return 1;
}
