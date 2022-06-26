/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * get_line.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "internal.h"

static void ltrim(char *buffer)
{
	size_t i = 0;

	while (isspace(buffer[i]))
		++i;

	if (i > 0)
		memmove(buffer, buffer + i, strlen(buffer + i) + 1);
}

static void rtrim(char *buffer)
{
	size_t i = strlen(buffer);

	while (i > 0 && isspace(buffer[i - 1]))
		--i;

	buffer[i] = '\0';
}

static size_t trim(char *buffer, int flags)
{
	if (flags & ISTREAM_LINE_LTRIM)
		ltrim(buffer);

	if (flags & ISTREAM_LINE_RTRIM)
		rtrim(buffer);

	return strlen(buffer);
}

int istream_get_line(istream_t *strm, char **out,
		     size_t *line_num, int flags)
{
	char *line = NULL, *new;
	size_t i, line_len = 0;
	bool have_line = false;

	*out = NULL;

	for (;;) {
		if (istream_precache(strm))
			return -1;

		if (strm->buffer_used == 0) {
			if (line_len == 0)
				goto out_eof;

			line_len = trim(line, flags);

			if (line_len == 0 &&
			    (flags & ISTREAM_LINE_SKIP_EMPTY)) {
				goto out_eof;
			}
			break;
		}

		for (i = 0; i < strm->buffer_used; ++i) {
			if (strm->buffer[i] == '\n')
				break;
		}

		if (i < strm->buffer_used) {
			have_line = true;
			strm->buffer_offset = i + 1;

			if (i > 0 && strm->buffer[i - 1] == '\r')
				--i;
		} else {
			strm->buffer_offset = i;
		}

		new = realloc(line, line_len + i + 1);
		if (new == NULL)
			goto fail_errno;

		line = new;
		memcpy(line + line_len, strm->buffer, i);
		line_len += i;
		line[line_len] = '\0';

		if (have_line) {
			line_len = trim(line, flags);

			if (line_len == 0 &&
			    (flags & ISTREAM_LINE_SKIP_EMPTY)) {
				free(line);
				line = NULL;
				have_line = false;
				*line_num += 1;
				continue;
			}
			break;
		}
	}

	*out = line;
	return 0;
fail_errno:
	fprintf(stderr, "%s: " PRI_SZ ": %s.\n", strm->get_filename(strm),
		*line_num, strerror(errno));
	free(line);
	*out = NULL;
	return -1;
out_eof:
	free(line);
	*out = NULL;
	return 1;
}
