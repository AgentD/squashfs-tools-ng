/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * split_line.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/parse.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static split_line_t *append_arg(split_line_t *in, char *arg)
{
	split_line_t *out = realloc(in, sizeof(*in) +
				    (in->count + 1) * sizeof(char *));

	if (out == NULL) {
		free(in);
		return NULL;
	}

	out->args[out->count++] = arg;
	return out;
}

static int is_sep(const char *sep, int c)
{
	return strchr(sep, c) != NULL && c != '\0';
}

int split_line(char *line, size_t len, const char *sep, split_line_t **out)
{
	split_line_t *split = calloc(1, sizeof(*split));
	char *src = line, *dst = line;

	if (split == NULL)
		return SPLIT_LINE_ALLOC;

	while (len > 0 && is_sep(sep, *src)) {
		++src;
		--len;
	}

	while (len > 0 && *src != '\0') {
		split = append_arg(split, dst);
		if (split == NULL)
			return SPLIT_LINE_ALLOC;

		if (*src == '"') {
			++src;
			--len;

			while (len > 0 && *src != '\0' && *src != '"') {
				if (src[0] == '\\') {
					if (len < 2)
						goto fail_esc;
					if (src[1] != '"' && src[1] != '\\')
						goto fail_esc;

					*(dst++) = src[1];
					src += 2;
					len -= 2;
				} else {
					*(dst++) = *(src++);
					--len;
				}
			}

			if (len == 0 || *src != '"')
				goto fail_quote;
			++src;
			--len;
		} else {
			while (len > 0 && !is_sep(sep, *src) && *src != '\0') {
				*(dst++) = *(src++);
				--len;
			}
		}

		while (len > 0 && is_sep(sep, *src)) {
			++src;
			--len;
		}

		*(dst++) = '\0';
	}

	*out = split;
	return SPLIT_LINE_OK;
fail_esc:
	free(split);
	return SPLIT_LINE_ESCAPE;
fail_quote:
	free(split);
	return SPLIT_LINE_UNMATCHED_QUOTE;
}

void split_line_remove_front(split_line_t *split, size_t count)
{
	if (count < split->count) {
		for (size_t i = count, j = 0; i < split->count; ++i, ++j)
			split->args[j] = split->args[i];
		split->count -= count;
	} else {
		split->count = 0;
	}
}
