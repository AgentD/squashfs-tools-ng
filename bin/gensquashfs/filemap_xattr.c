/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filemap_xattr.c
 *
 * Copyright (C) 2022 Enno Boland <mail@eboland.de>
 */
#include "fstree.h"
#include "mkfs.h"
#include <stdio.h>

#define NEW_FILE_START "# file: "

// Taken from attr-2.5.1/tools/setfattr.c
static char *
decode(const char *value, size_t *size) {
	char *decoded = NULL;

	if (*size == 0)
		return strdup("");
	if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
		*size = ((*size) - 2) / 2;

		decoded = realloc(decoded, *size);
		if (decoded == NULL) {
			return NULL;
		}

		if (hex_decode(value + 2, (*size) * 2, decoded, *size)) {
			fprintf(stderr, "bad input encoding\n");
			free(decoded);
			return NULL;
		}
	} else if (value[0] == '0' && (value[1] == 's' || value[1] == 'S')) {
		size_t input_len = *size - 2;

		*size = (input_len / 4) * 3;

		decoded = realloc(decoded, *size);
		if (decoded == NULL) {
			return NULL;
		}

		if (base64_decode(value + 2, input_len, decoded, size)) {
			free(decoded);
			fprintf(stderr, "bad input encoding\n");
			return NULL;
		}
	} else {
		const char *v = value, *end = value + *size;
		char *d;

		if (end > v + 1 && *v == '"' && *(end - 1) == '"') {
			v++;
			end--;
		}

		decoded = realloc(decoded, *size);
		if (decoded == NULL) {
			return NULL;
		}
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
}

static int
parse_file_name(char *line, struct XattrMap *map) {
	char *p;
	struct XattrMapPattern *current_file;
	char *file_name = &line[strlen(NEW_FILE_START)];

	p = strchr(file_name, '\n');
	if (p != NULL) {
		*p = '\0';
	}
	file_name = strdup(file_name);
	if (file_name == NULL) {
		return -1;
	}
	current_file = calloc(1, sizeof(struct XattrMapPattern));
	if (current_file == NULL) {
		free(file_name);
		return -1;
	}

	current_file->next = map->patterns;
	map->patterns = current_file;
	canonicalize_name(file_name);
	current_file->path = file_name;

	return 0;
}

static int
parse_xattr(char *key_start, char *value_start, struct XattrMap *map) {
	char *p;
	size_t len;
	struct XattrMapPattern *current_pattern = map->patterns;
	struct XattrMapEntry *current_entry;

	*value_start = '\0';
	value_start += 1;
	p = strchr(value_start, '\n');
	if (p != NULL) {
		*p = '\0';
	}

	current_entry = calloc(1, sizeof(struct XattrMapEntry));
	if (current_entry == NULL) {
		return -1;
	}
	current_entry->next = current_pattern->entries;
	current_pattern->entries = current_entry;

	current_entry->key = strdup(key_start);
	len = strlen(value_start);
	current_entry->value = decode(value_start, &len);
	current_entry->value_len = len;

	return 0;
}

void *
xattr_open_map_file(const char *path) {
	struct XattrMap *map;
	char *line = NULL;
	size_t line_size;
	char *p = NULL;
	FILE *file = fopen(path, "r");
	if (file == NULL) {
		return NULL;
	}

	map = calloc(1, sizeof(struct XattrMap));
	if (map == NULL) {
		fclose(file);
		return NULL;
	}

	while (getline(&line, &line_size, file) != -1) {
		if (strncmp(NEW_FILE_START, line, strlen(NEW_FILE_START)) == 0) {
			if (parse_file_name(line, map) < 0) {
				fclose(file);
				xattr_close_map_file(map);
				return NULL;
			}
		} else if ((p = strchr(line, '=')) && map->patterns) {
			if (parse_xattr(line, p, map) < 0) {
				fclose(file);
				xattr_close_map_file(map);
				return NULL;
			}
		}
	}

	free(line);
	fclose(file);
	return map;
}

void
xattr_close_map_file(void *xattr_map) {
	struct XattrMap *map = xattr_map;
	while (map->patterns != NULL) {
		struct XattrMapPattern *file = map->patterns;
		map->patterns = file->next;
		while (file->entries != NULL) {
			struct XattrMapEntry *entry = file->entries;
			file->entries = entry->next;
			free(entry->key);
			entry->key = NULL;
			free(entry->value);
			entry->value = NULL;
			free(entry);
		}
		free(file->path);
		file->path = NULL;
		free(file);
	}
	free(xattr_map);
}

int
xattr_apply_map_file(char *path, void *map, sqfs_xattr_writer_t *xwr) {
	struct XattrMap *xattr_map = map;
	int ret = 0;
	const struct XattrMapPattern *pat;
	const struct XattrMapEntry *entry;

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
				ret = sqfs_xattr_writer_add(
						xwr, entry->key, entry->value, entry->value_len);
				if (ret < 0) {
					return ret;
				}
			}
		}
	}
	return ret;
}
