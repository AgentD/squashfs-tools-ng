/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * fix_win32_filename.c
 *
 * Copyright (C) 2024 David Oberhollenzer <goliath@infraroot.at>
 */
#include "util/util.h"

#include <string.h>
#include <stdlib.h>

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#endif

typedef struct {
	size_t used;
	size_t available;
	char buffer[];
} buffer_t;

static buffer_t *buffer_append(buffer_t *buf, const char *data, size_t count)
{
	size_t bufspace, needed;

	if (buf == NULL) {
		buf = calloc(1, sizeof(*buf) + 128);
		if (buf == NULL)
			return NULL;

		buf->used = 1;
		buf->available = 128;
		buf->buffer[0] = '\0';
	}

	bufspace = buf->available;
	needed = buf->used + count;

	while (bufspace < needed)
		bufspace += 128;

	if (bufspace != buf->available) {
		void *new_buf = realloc(buf, sizeof(*buf) + bufspace);
		if (new_buf == NULL) {
			free(buf);
			return NULL;
		}
		buf = new_buf;
		buf->available = bufspace;
	}

	buf->used -= 1;
	memcpy(buf->buffer + buf->used, data, count);
	buf->used += count;
	buf->buffer[buf->used++] = '\0';
	return buf;
}

static const char *bad_names[] = {
	"CON", "PRN", "AUX", "NUL",
	"COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
	"LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
};

static buffer_t *handle_component(buffer_t *buf, const char *comp, size_t len)
{
	for (size_t i = 0; i < sizeof(bad_names) / sizeof(bad_names[0]); ++i) {
		if (!strncasecmp(comp, bad_names[i], len)) {
			buf = buffer_append(buf, comp, len);
			if (buf != NULL)
				buf = buffer_append(buf, "_", 1);
			return buf;
		}
	}

	while (len > 0) {
		sqfs_u8 value, rep[3];
		size_t i = 0;

		for (i = 0; i < len; ++i) {
			if (comp[i] < 0x20 || comp[i] == 0x7F)
				break;
			if (comp[i] == '<' || comp[i] == '>' || comp[i] == ':')
				break;
			if (comp[i] == '|' || comp[i] == '?' || comp[i] == '*')
				break;
			if (comp[i] == '\\' || comp[i] == '\"')
				break;
		}

		if (i > 0) {
			buf = buffer_append(buf, comp, i);
			if (buf == NULL || i == len)
				break;
		}

		value = comp[i++];
		comp += i;
		len -= i;

		rep[0] = 0xEF;
		rep[1] = 0x80 | ((value >> 6) & 0x3f);
		rep[2] = 0x80 | ( value       & 0x3f);

		buf = buffer_append(buf, (const char *)rep, 3);
		if (buf == NULL)
			break;
	}

	return buf;
}

static buffer_t *handle_name(buffer_t *buf, const char *name, size_t len)
{
	char *sep;

	while ((sep = memchr(name, '.', len)) != NULL) {
		buf = handle_component(buf, name, sep - name);
		if (buf == NULL)
			return NULL;

		buf = buffer_append(buf, ".", 1);
		if (buf == NULL)
			return NULL;

		len -= sep - name + 1;
		name = sep + 1;
	}

	return handle_component(buf, name, len);
}

char *fix_win32_filename(const char *path)
{
	buffer_t *buf = NULL;
	char *sep, *out;
	size_t len;

	while ((sep = strchr(path, '/')) != NULL) {
		buf = handle_name(buf, path, sep - path);
		if (buf == NULL)
			return NULL;

		buf = buffer_append(buf, "/", 1);
		if (buf == NULL)
			return NULL;

		path = sep + 1;
	}

	buf = handle_name(buf, path, strlen(path));
	if (buf == NULL)
		return NULL;

	len = buf->used;
	memmove(buf, buf->buffer, len);

	out = realloc(buf, len);
	if (out == NULL)
		out = (char *)buf;

	return out;
}
