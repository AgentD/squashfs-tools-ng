/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * glob.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static const struct {
	const char *name;
	unsigned int clear_flag;
	unsigned int set_flag;
} glob_scan_flags[] = {
	{ "-type b", DIR_SCAN_NO_BLK, 0 },
	{ "-type c", DIR_SCAN_NO_CHR, 0 },
	{ "-type d", DIR_SCAN_NO_DIR, 0 },
	{ "-type p", DIR_SCAN_NO_FIFO, 0 },
	{ "-type f", DIR_SCAN_NO_FILE, 0 },
	{ "-type l", DIR_SCAN_NO_SLINK, 0 },
	{ "-type s", DIR_SCAN_NO_SOCK, 0 },
	{ "-xdev", 0, DIR_SCAN_ONE_FILESYSTEM },
	{ "-mount", 0, DIR_SCAN_ONE_FILESYSTEM },
	{ "-keeptime", 0, DIR_SCAN_KEEP_TIME },
	{ "-nonrecursive", 0, DIR_SCAN_NO_RECURSION },
};

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

int glob_files(fstree_t *fs, const char *filename, size_t line_num,
	       const char *path, struct stat *basic,
	       const char *basepath, unsigned int glob_flags,
	       const char *extra)
{
	char *name_pattern = NULL, *prefix = NULL;
	unsigned int scan_flags = 0, all_flags;
	dir_iterator_t *dir = NULL;
	bool first_clear_flag;
	size_t i, count, len;
	dir_tree_cfg_t cfg;
	tree_node_t *root;
	int ret;

	/* fetch the actual target node */
	root = fstree_get_node_by_path(fs, fs->root, path, true, false);
	if (root == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s: %s\n",
			filename, line_num, path, strerror(errno));
		return -1;
	}

	if (!S_ISDIR(root->mode)) {
		fprintf(stderr, "%s: " PRI_SZ ": %s is not a directoy!\n",
			filename, line_num, path);
		return -1;
	}

	prefix = fstree_get_path(root);
	if (canonicalize_name(prefix) != 0) {
		fprintf(stderr, "%s: " PRI_SZ ": error cannonicalizing `%s`!\n",
			filename, line_num, prefix);
		free(prefix);
		return -1;
	}

	/* process options */
	first_clear_flag = true;

	all_flags = DIR_SCAN_NO_BLK | DIR_SCAN_NO_CHR | DIR_SCAN_NO_DIR |
		DIR_SCAN_NO_FIFO | DIR_SCAN_NO_FILE | DIR_SCAN_NO_SLINK |
		DIR_SCAN_NO_SOCK;

	while (extra != NULL && *extra != '\0') {
		count = sizeof(glob_scan_flags) / sizeof(glob_scan_flags[0]);

		for (i = 0; i < count; ++i) {
			len = strlen(glob_scan_flags[i].name);
			if (strncmp(extra, glob_scan_flags[i].name, len) != 0)
				continue;

			if (isspace(extra[len])) {
				extra += len;
				while (isspace(*extra))
					++extra;
				break;
			}
		}

		if (i < count) {
			if (glob_scan_flags[i].clear_flag != 0 &&
			    first_clear_flag) {
				scan_flags |= all_flags;
				first_clear_flag = false;
			}

			scan_flags &= ~(glob_scan_flags[i].clear_flag);
			scan_flags |= glob_scan_flags[i].set_flag;
			continue;
		}

		if (strncmp(extra, "-name", 5) == 0 && isspace(extra[5])) {
			for (extra += 5; isspace(*extra); ++extra)
				;

			len = name_string_length(extra);

			free(name_pattern);
			name_pattern = strndup(extra, len);
			extra += len;

			while (isspace(*extra))
				++extra;

			quote_remove(name_pattern);
			continue;
		}

		if (strncmp(extra, "-path", 5) == 0 && isspace(extra[5])) {
			for (extra += 5; isspace(*extra); ++extra)
				;

			len = name_string_length(extra);

			free(name_pattern);
			name_pattern = strndup(extra, len);
			extra += len;

			while (isspace(*extra))
				++extra;

			quote_remove(name_pattern);
			glob_flags |= DIR_SCAN_MATCH_FULL_PATH;
			continue;
		}

		if (extra[0] == '-') {
			if (extra[1] == '-' && isspace(extra[2])) {
				extra += 2;
				while (isspace(*extra))
					++extra;
				break;
			}

			fprintf(stderr, "%s: " PRI_SZ ": unknown option.\n",
				filename, line_num);
			free(name_pattern);
			free(prefix);
			return -1;
		} else {
			break;
		}
	}

	if (extra != NULL && *extra == '\0')
		extra = NULL;

	/* do the scan */
	memset(&cfg, 0, sizeof(cfg));
	cfg.flags = scan_flags | glob_flags;
	cfg.def_mtime = basic->st_mtime;
	cfg.def_uid = basic->st_uid;
	cfg.def_gid = basic->st_gid;
	cfg.def_mode = basic->st_mode;
	cfg.prefix = prefix;
	cfg.name_pattern = name_pattern;

	if (basepath == NULL) {
		dir = dir_tree_iterator_create(extra == NULL ? "." : extra,
					       &cfg);
	} else {
		size_t plen = strlen(basepath);
		size_t slen = strlen(extra);
		char *temp = calloc(1, plen + 1 + slen + 1);

		if (temp == NULL) {
			fprintf(stderr, "%s: " PRI_SZ ": allocation failure.\n",
				filename, line_num);
		} else {
			memcpy(temp, basepath, plen);
			temp[plen] = '/';
			memcpy(temp + plen + 1, extra, slen);
			temp[plen + 1 + slen] = '\0';

			dir = dir_tree_iterator_create(temp, &cfg);
		}

		free(temp);
	}

	if (dir != NULL) {
		ret = fstree_from_dir(fs, dir);
		sqfs_drop(dir);
	} else {
		ret = -1;
	}

	free(name_pattern);
	free(prefix);
	return ret;
}
