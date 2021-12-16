/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * filemap_xattr.c
 *
 * Copyright (C) 2021 Enno Boland <mail@eboland.de>
 */
#include "mkfs.h"
#include <stdio.h>

#define NEW_FILE_START "# file: "

struct XattrMapEntry {
	char *key;
	char *value;
	struct XattrMapEntry *next;
};

struct XattrMapFile {
	char *path;
	bool is_glob;
	struct XattrMapEntry *entries;
	struct XattrMapFile *next;
};

struct XattrMap {
	struct XattrMapFile *files;
};

static int parse_file_name(char *line, struct XattrMap *map) {
	char *p;
	struct XattrMapFile *current_file = map->files;
	char *file_name = &line[strlen(NEW_FILE_START)];

	p = strchr(file_name, '\n');
	if (p != NULL) {
		*p = '\0';
	}
	// printf("%s\n", file_name);
	current_file = calloc(1, sizeof(struct XattrMapFile));
	if (current_file == NULL) {
		return -1;
	}
	current_file->next = map->files;
	map->files = current_file;
	current_file->path = strdup(file_name);

	return 0;
}

static int
parse_xattr(char *key_start, char* value_start, struct XattrMap *map) {
	char *p;
	struct XattrMapFile *current_file = map->files;
	struct XattrMapEntry *current_entry = current_file->entries;

	*value_start = '\0';
	printf("%s\n", key_start);
	value_start += 1;
	p = strchr(value_start, '\n');
	if (p != NULL) {
		*p = '\0';
	}
	printf("%s\n", value_start);

	current_entry = calloc(1, sizeof(struct XattrMapEntry));
	if (current_entry == NULL) {
		return -1;
	}
	current_entry->next = current_file->entries;
	current_file->entries = current_entry;

	current_entry->key = strdup(key_start);
	current_entry->value = strdup(value_start);

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
		} else if ((p = strchr(line, '=')) && map->files) {
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

int
xattrs_from_map_file(
		fstree_t *fs, const char *path, void *mapfile,
		sqfs_xattr_writer_t *xwr) {
	(void)fs;
	(void)path;
	(void)mapfile;
	(void)xwr;
	if (xwr == NULL)
		return 0;
	return 0;
}

void
xattr_close_map_file(void *xattr_map) {
	free(xattr_map);
}
