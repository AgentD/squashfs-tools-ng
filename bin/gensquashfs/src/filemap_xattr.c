/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filemap_xattr.c
 *
 * Copyright (C) 2022 Enno Boland <mail@eboland.de>
 */
#include "mkfs.h"

#define NEW_FILE_START "# file: "

static void print_error(const char *filename, size_t line_num, const char *err)
{
	fprintf(stderr, "%s: " PRI_SZ ": %s\n", filename, line_num, err);
}

// Taken from attr-2.5.1/tools/setfattr.c
static sqfs_u8 *decode(const char *filename, size_t line_num,
		       const char *value, size_t *size)
{
	sqfs_u8 *decoded = NULL;

	if (*size == 0) {
		decoded = (sqfs_u8 *)strdup("");
		if (decoded == NULL)
			goto fail_alloc;
		return decoded;
	}

	if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
		*size = ((*size) - 2) / 2;

		decoded = calloc(1, (*size) + 1);
		if (decoded == NULL)
			goto fail_alloc;

		if (hex_decode(value + 2, (*size) * 2, decoded, *size))
			goto fail_encode;
	} else if (value[0] == '0' && (value[1] == 's' || value[1] == 'S')) {
		size_t input_len = *size - 2;

		*size = (input_len / 4) * 3;

		decoded = calloc(1, (*size) + 1);
		if (decoded == NULL)
			goto fail_alloc;

		if (base64_decode(value + 2, input_len, decoded, size))
			goto fail_encode;
	} else {
		const char *v = value, *end = value + *size;
		sqfs_u8 *d;

		if (end > v + 1 && *v == '"' && *(end - 1) == '"') {
			v++;
			end--;
		}

		decoded = calloc(1, (*size) + 1);
		if (decoded == NULL)
			goto fail_alloc;

		d = decoded;

		while (v < end) {
			if (v[0] == '\\') {
				if (v[1] == '\\' || v[1] == '"') {
					*d++ = *++v;
					v++;
				} else if (v[1] >= '0' && v[1] <= '7') {
					int c = 0;
					v++;
					c = (*v++ - '0');
					if (*v >= '0' && *v <= '7')
						c = (c << 3) + (*v++ - '0');
					if (*v >= '0' && *v <= '7')
						c = (c << 3) + (*v++ - '0');
					*d++ = c;
				} else
					*d++ = *v++;
			} else
				*d++ = *v++;
		}
		*size = d - decoded;
	}
	return decoded;
fail_alloc:
	fprintf(stderr, "out of memory\n");
	return NULL;
fail_encode:
	print_error(filename, line_num, "bad input encoding");
	free(decoded);
	return NULL;
}

static int parse_file_name(const char *filename, size_t line_num,
			   char *line, struct XattrMap *map)
{
	struct XattrMapPattern *current_file;
	char *file_name = strdup(line + strlen(NEW_FILE_START));

	if (file_name == NULL)
		goto fail_alloc;

	current_file = calloc(1, sizeof(struct XattrMapPattern));
	if (current_file == NULL)
		goto fail_alloc;

	current_file->next = map->patterns;
	map->patterns = current_file;

	if (canonicalize_name(file_name)) {
		print_error(filename, line_num, "invalid absolute path");
		free(current_file);
		free(file_name);
		return -1;
	}

	current_file->path = file_name;
	return 0;
fail_alloc:
	fprintf(stderr, "out of memory\n");
	free(file_name);
	return -1;
}

static int parse_xattr(const char *filename, size_t line_num, char *key_start,
		       char *value_start, struct XattrMap *map)
{
	size_t len;
	struct XattrMapPattern *current_pattern = map->patterns;
	sqfs_xattr_t *current_entry;
	sqfs_u8 *value;

	if (current_pattern == NULL) {
		print_error(filename, line_num, "no file specified yet");
		return -1;
	}

	len = strlen(value_start);
	value = decode(filename, line_num, value_start, &len);
	if (value == NULL)
		return -1;

	current_entry = sqfs_xattr_create(key_start, value, len);
	free(value);
	if (current_entry == NULL) {
		print_error(filename, line_num, "out-of-memory");
		return -1;
	}

	current_entry->next = current_pattern->entries;
	current_pattern->entries = current_entry;

	return 0;
}

void *
xattr_open_map_file(const char *path) {
	struct XattrMap *map;
	size_t line_num = 1;
	char *p = NULL;
	istream_t *file = istream_open_file(path);
	if (file == NULL) {
		return NULL;
	}

	map = calloc(1, sizeof(struct XattrMap));
	if (map == NULL)
		goto fail_close;

	for (;;) {
		char *line = NULL;
		int ret = istream_get_line(file, &line, &line_num,
					   ISTREAM_LINE_LTRIM |
					   ISTREAM_LINE_RTRIM |
					   ISTREAM_LINE_SKIP_EMPTY);
		if (ret < 0)
			goto fail;
		if (ret > 0)
			break;

		if (strncmp(NEW_FILE_START, line, strlen(NEW_FILE_START)) == 0) {
			ret = parse_file_name(path, line_num, line, map);
		} else if ((p = strchr(line, '='))) {
			*(p++) = '\0';
			ret = parse_xattr(path, line_num, line, p, map);
		} else if (line[0] != '#') {
			print_error(path, line_num, "not a key-value pair");
			ret = -1;
		}

		++line_num;
		free(line);
		if (ret < 0)
			goto fail;
	}

	sqfs_drop(file);
	return map;
fail:
	xattr_close_map_file(map);
fail_close:
	sqfs_drop(file);
	return NULL;
}

void
xattr_close_map_file(void *xattr_map) {
	struct XattrMap *map = xattr_map;
	while (map->patterns != NULL) {
		struct XattrMapPattern *file = map->patterns;
		map->patterns = file->next;

		sqfs_xattr_list_free(file->entries);
		free(file->path);
		free(file);
	}
	free(xattr_map);
}

int
xattr_apply_map_file(char *path, void *map, sqfs_xattr_writer_t *xwr) {
	struct XattrMap *xattr_map = map;
	int ret = 0;
	const struct XattrMapPattern *pat;
	const sqfs_xattr_t *entry;

	for (pat = xattr_map->patterns; pat != NULL; pat = pat->next) {
		char *patstr = pat->path;
		const char *stripped = path;

		if (patstr[0] != '/' && stripped[0] == '/') {
			stripped++;
		}

		if (strcmp(patstr, stripped) == 0) {
			printf("Applying xattrs for %s", path);
			for (entry = pat->entries; entry != NULL; entry = entry->next) {
				printf("  %s = \n", entry->key);
				fwrite(entry->value, entry->value_len, 1, stdout);
				puts("\n");
				ret = sqfs_xattr_writer_add(xwr, entry);
				if (ret < 0) {
					return ret;
				}
			}
		}
	}
	return ret;
}
