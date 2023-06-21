/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * glob.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static const struct {
	char c;
	unsigned int flag;
} glob_types[] = {
	{ 'b', DIR_SCAN_NO_BLK },   { 'B', DIR_SCAN_NO_BLK },
	{ 'c', DIR_SCAN_NO_CHR },   { 'C', DIR_SCAN_NO_CHR },
	{ 'd', DIR_SCAN_NO_DIR },   { 'D', DIR_SCAN_NO_DIR },
	{ 'p', DIR_SCAN_NO_FIFO },  { 'P', DIR_SCAN_NO_FIFO },
	{ 'f', DIR_SCAN_NO_FILE },  { 'F', DIR_SCAN_NO_FILE },
	{ 'l', DIR_SCAN_NO_SLINK }, { 'L', DIR_SCAN_NO_SLINK },
	{ 's', DIR_SCAN_NO_SOCK },  { 'S', DIR_SCAN_NO_SOCK },
};

static const struct {
	const char *name;
	unsigned int flag;
} glob_scan_flags[] = {
	{ "-xdev", DIR_SCAN_ONE_FILESYSTEM },
	{ "-mount", DIR_SCAN_ONE_FILESYSTEM },
	{ "-keeptime", DIR_SCAN_KEEP_TIME },
	{ "-nonrecursive", DIR_SCAN_NO_RECURSION },
};

static void set_all_type_flags(unsigned int *flags)
{
	size_t count = sizeof(glob_types) / sizeof(glob_types[0]);

	for (size_t i = 0; i < count; ++i)
		*flags |= glob_types[i].flag;
}

static size_t name_string_length(const char *str)
{
	size_t len = 0;
	int start;

	if (*str == '"' || *str == '\'') {
		start = *str;
		++len;

		while (str[len] != '\0' && str[len] != start)
			++len;

		if (str[len] == start)
			++len;
	} else {
		while (str[len] != '\0' && !isspace(str[len]))
			++len;
	}

	return len;
}

static void quote_remove(char *str)
{
	char *dst = str;
	int start = *(str++);

	if (start != '\'' && start != '"')
		return;

	while (*str != start && *str != '\0')
		*(dst++) = *(str++);

	*(dst++) = '\0';
}

static const char *skip_space(const char *str)
{
	while (isspace(*str))
		++str;
	return str;
}

static bool match_option(const char **line, const char *candidate)
{
	size_t len = strlen(candidate);

	if (strncmp(*line, candidate, len) != 0 || !isspace((*line)[len]))
		return false;

	*line = skip_space(*line + len);
	return true;
}

int glob_files(fstree_t *fs, const char *filename, size_t line_num,
	       const dir_entry_t *ent,
	       const char *basepath, unsigned int glob_flags,
	       const char *extra)
{
	char *name_pattern = NULL, *prefix = NULL;
	unsigned int scan_flags = 0;
	dir_iterator_t *dir = NULL;
	bool first_clear_flag;
	dir_tree_cfg_t cfg;
	tree_node_t *root;
	int ret;

	/* fetch the actual target node */
	root = fstree_get_node_by_path(fs, fs->root, ent->name, true, false);
	if (root == NULL)
		goto fail_path;

	if (!S_ISDIR(root->mode))
		goto fail_not_dir;

	prefix = fstree_get_path(root);
	if (prefix == NULL)
		goto fail_alloc;

	if (canonicalize_name(prefix) != 0)
		goto fail_prefix;

	/* process options */
	first_clear_flag = true;

	while (extra != NULL && *extra == '-' && !match_option(&extra, "--")) {
		bool is_name = false, is_path = false, found = false;
		size_t count = sizeof(glob_scan_flags) /
			sizeof(glob_scan_flags[0]);

		for (size_t i = 0; i < count && !found; ++i) {
			if (match_option(&extra, glob_scan_flags[i].name)) {
				scan_flags |= glob_scan_flags[i].flag;
				found = true;
			}
		}

		if (found)
			continue;

		if (match_option(&extra, "-type")) {
			char c = *extra;

			if (!isalpha(c))
				goto fail_type;
			if (!isspace(extra[1]) && extra[1] != '\0')
				goto fail_type;

			count = sizeof(glob_types) / sizeof(glob_types[0]);

			if (first_clear_flag) {
				set_all_type_flags(&scan_flags);
				first_clear_flag = false;
			}

			for (size_t i = 0; i < count && !found; ++i) {
				if (glob_types[i].c == c) {
					scan_flags &= ~(glob_types[i].flag);
					found = true;
				}
			}

			if (!found)
				goto fail_type;

			extra = skip_space(extra + 1);
			continue;
		}

		if (match_option(&extra, "-name")) {
			is_name = true;
		} else if (match_option(&extra, "-path")) {
			is_path = true;
		}

		if (is_name || is_path) {
			size_t len = name_string_length(extra);
			free(name_pattern);
			name_pattern = strndup(extra, len);
			if (name_pattern == NULL)
				goto fail_alloc;

			extra = skip_space(extra + len);

			quote_remove(name_pattern);
			if (is_path)
				glob_flags |= DIR_SCAN_MATCH_FULL_PATH;
			continue;
		}

		goto fail_unknown;
	}

	if (extra != NULL && *extra == '\0')
		extra = NULL;

	/* do the scan */
	memset(&cfg, 0, sizeof(cfg));
	cfg.flags = scan_flags | glob_flags;
	cfg.def_mtime = ent->mtime;
	cfg.def_uid = ent->uid;
	cfg.def_gid = ent->gid;
	cfg.def_mode = ent->mode;
	cfg.prefix = prefix;
	cfg.name_pattern = name_pattern;

	if (basepath == NULL) {
		dir = dir_tree_iterator_create(extra == NULL ? "." : extra,
					       &cfg);
	} else {
		size_t plen = strlen(basepath);
		size_t slen = strlen(extra);
		char *temp = calloc(1, plen + 1 + slen + 1);

		if (temp == NULL)
			goto fail_alloc;

		memcpy(temp, basepath, plen);
		temp[plen] = '/';
		memcpy(temp + plen + 1, extra, slen);
		temp[plen + 1 + slen] = '\0';

		dir = dir_tree_iterator_create(temp, &cfg);
		free(temp);
	}

	if (dir == NULL)
		goto fail;

	ret = fstree_from_dir(fs, dir);
	sqfs_drop(dir);

	free(name_pattern);
	free(prefix);
	return ret;
fail_unknown:
	fprintf(stderr, "%s: " PRI_SZ ": unknown glob option: %s.\n",
		filename, line_num, extra);
	goto fail;
fail_path:
	fprintf(stderr, "%s: " PRI_SZ ": %s: %s\n",
		filename, line_num, ent->name, strerror(errno));
	goto fail;
fail_not_dir:
	fprintf(stderr, "%s: " PRI_SZ ": %s is not a directoy!\n",
		filename, line_num, ent->name);
	goto fail;
fail_prefix:
	fprintf(stderr, "%s: " PRI_SZ ": error cannonicalizing `%s`!\n",
		filename, line_num, prefix);
	goto fail;
fail_alloc:
	fprintf(stderr, "%s: " PRI_SZ ": out of memory\n",
		filename, line_num);
	goto fail;
fail_type:
	fprintf(stderr, "%s: " PRI_SZ ": unknown file type `%.3s...`\n",
		filename, line_num, extra);
	goto fail;
fail:
	free(name_pattern);
	free(prefix);
	return -1;
}
