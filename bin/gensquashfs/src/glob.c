/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * glob.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static const struct {
	const char *name;
	unsigned int flag;
} glob_types[] = {
	{ "b", DIR_SCAN_NO_BLK },   { "B", DIR_SCAN_NO_BLK },
	{ "c", DIR_SCAN_NO_CHR },   { "C", DIR_SCAN_NO_CHR },
	{ "d", DIR_SCAN_NO_DIR },   { "D", DIR_SCAN_NO_DIR },
	{ "p", DIR_SCAN_NO_FIFO },  { "P", DIR_SCAN_NO_FIFO },
	{ "f", DIR_SCAN_NO_FILE },  { "F", DIR_SCAN_NO_FILE },
	{ "l", DIR_SCAN_NO_SLINK }, { "L", DIR_SCAN_NO_SLINK },
	{ "s", DIR_SCAN_NO_SOCK },  { "S", DIR_SCAN_NO_SOCK },
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

int glob_files(fstree_t *fs, const char *filename, size_t line_num,
	       const dir_entry_t *ent,
	       const char *basepath, unsigned int glob_flags,
	       char *extra)
{
	const char *name_pattern = NULL;
	char *prefix = NULL;
	unsigned int scan_flags = 0;
	dir_iterator_t *dir = NULL;
	bool first_clear_flag;
	dir_tree_cfg_t cfg;
	tree_node_t *root;
	split_line_t *sep;
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
	switch (split_line(extra, strlen(extra), " \t", &sep)) {
	case SPLIT_LINE_OK:              break;
	case SPLIT_LINE_ALLOC:           goto fail_alloc;
	case SPLIT_LINE_UNMATCHED_QUOTE: goto fail_quote;
	case SPLIT_LINE_ESCAPE:          goto fail_esc;
	default:                         goto fail_split;
	}

	first_clear_flag = true;

	while (sep->count != 0) {
		bool is_name = false, is_path = false, found = false;
		size_t count;

		if (sep->args[0][0] != '-')
			break;

		if (!strcmp(sep->args[0], "--")) {
			split_line_remove_front(sep, 1);
			break;
		}

		count = sizeof(glob_scan_flags) / sizeof(glob_scan_flags[0]);

		for (size_t i = 0; i < count && !found; ++i) {
			if (!strcmp(sep->args[0], glob_scan_flags[i].name)) {
				scan_flags |= glob_scan_flags[i].flag;
				found = true;
			}
		}

		if (found) {
			split_line_remove_front(sep, 1);
			continue;
		}

		if (!strcmp(sep->args[0], "-type")) {
			if (sep->count < 2)
				goto fail_no_arg;

			if (first_clear_flag) {
				set_all_type_flags(&scan_flags);
				first_clear_flag = false;
			}

			count = sizeof(glob_types) / sizeof(glob_types[0]);

			for (size_t i = 0; i < count && !found; ++i) {
				if (!strcmp(glob_types[i].name, sep->args[1])) {
					scan_flags &= ~(glob_types[i].flag);
					found = true;
				}
			}

			if (!found)
				goto fail_type;

			split_line_remove_front(sep, 2);
			continue;
		}

		if (!strcmp(sep->args[0], "-name")) {
			is_name = true;
		} else if (!strcmp(sep->args[0], "-path")) {
			is_path = true;
		}

		if (is_name || is_path) {
			if (sep->count < 2)
				goto fail_no_arg;

			name_pattern = sep->args[1];
			if (is_path)
				glob_flags |= DIR_SCAN_MATCH_FULL_PATH;

			split_line_remove_front(sep, 2);
			continue;
		}

		goto fail_unknown;
	}

	/* do the scan */
	memset(&cfg, 0, sizeof(cfg));
	cfg.flags = scan_flags | glob_flags;
	cfg.def_mtime = ent->mtime;
	cfg.def_uid = ent->uid;
	cfg.def_gid = ent->gid;
	cfg.def_mode = ent->mode;
	cfg.prefix = prefix;
	cfg.name_pattern = name_pattern;

	if (basepath == NULL && sep->count == 0) {
		dir = dir_tree_iterator_create(".", &cfg);
	} else if (basepath != NULL && sep->count == 0) {
		dir = dir_tree_iterator_create(basepath, &cfg);
	} else if (basepath == NULL && sep->count != 0) {
		dir = dir_tree_iterator_create(sep->args[0], &cfg);
	} else {
		size_t plen = strlen(basepath);
		size_t slen = strlen(sep->args[0]);
		char *temp = calloc(1, plen + 1 + slen + 1);

		if (temp == NULL)
			goto fail_alloc;

		memcpy(temp, basepath, plen);
		temp[plen] = '/';
		memcpy(temp + plen + 1, sep->args[0], slen);
		temp[plen + 1 + slen] = '\0';

		dir = dir_tree_iterator_create(temp, &cfg);
		free(temp);
	}

	if (dir == NULL)
		goto fail;

	ret = fstree_from_dir(fs, dir);
	sqfs_drop(dir);

	free(prefix);
	free(sep);
	return ret;
fail_quote:
	fprintf(stderr, "%s: " PRI_SZ ": unmatched `\"`.\n",
		filename, line_num);
	goto fail;
fail_esc:
	fprintf(stderr, "%s: " PRI_SZ ": broken escape sequence.\n",
		filename, line_num);
	goto fail;
fail_split:
	fprintf(stderr, "[BUG] %s: " PRI_SZ ": unknown error parsing line.\n",
		filename, line_num);
	goto fail;
fail_unknown:
	fprintf(stderr, "%s: " PRI_SZ ": unknown glob option: %s.\n",
		filename, line_num, sep->args[0]);
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
	fprintf(stderr, "%s: " PRI_SZ ": unknown file type `%s`\n",
		filename, line_num, sep->args[1]);
	goto fail;
fail_no_arg:
	fprintf(stderr, "%s: " PRI_SZ ": missing argument for `%s`\n",
		filename, line_num, sep->args[0]);
	goto fail;
fail:
	free(sep);
	free(prefix);
	return -1;
}
