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

static bool apply_type_flag(bool is_first, const char *arg,
			    unsigned int *flags)
{
	size_t count = sizeof(glob_types) / sizeof(glob_types[0]);

	if (is_first) {
		for (size_t i = 0; i < count; ++i)
			(*flags) |= glob_types[i].flag;
	}

	for (size_t i = 0; i < count; ++i) {
		if (!strcmp(glob_types[i].name, arg)) {
			(*flags) &= ~(glob_types[i].flag);
			return true;
		}
	}

	return false;
}

static const struct {
	const char *name;
	unsigned int flag;
} glob_scan_flags[] = {
	{ "-xdev", DIR_SCAN_ONE_FILESYSTEM },
	{ "-mount", DIR_SCAN_ONE_FILESYSTEM },
	{ "-keeptime", DIR_SCAN_KEEP_TIME },
	{ "-nonrecursive", DIR_SCAN_NO_RECURSION },
};

static bool set_scan_flag(const char *arg, dir_tree_cfg_t *cfg)
{
	size_t count = sizeof(glob_scan_flags) / sizeof(glob_scan_flags[0]);

	for (size_t i = 0; i < count; ++i) {
		if (!strcmp(arg, glob_scan_flags[i].name)) {
			cfg->flags |= glob_scan_flags[i].flag;
			return true;
		}
	}

	return false;
}

static int scan_directory(fstree_t *fs, sqfs_dir_iterator_t *dir,
			  size_t prefix_len, const char *file_prefix)
{
	for (;;) {
		sqfs_dir_entry_t *ent = NULL;
		tree_node_t *n = NULL;
		char *extra = NULL;

		int ret = dir->next(dir, &ent);
		if (ret > 0)
			break;
		if (ret < 0) {
			sqfs_perror("readdir", NULL, ret);
			return -1;
		}

		n = fstree_get_node_by_path(fs, fs->root, ent->name,
					    false, true);
		if (n == NULL) {
			if (S_ISDIR(ent->mode))
				dir->ignore_subdir(dir);
			free(ent);
			continue;
		}

		if (S_ISLNK(ent->mode)) {
			ret = dir->read_link(dir, &extra);
			if (ret) {
				free(ent);
				sqfs_perror("readlink", ent->name, ret);
				return -1;
			}
		} else if (S_ISREG(ent->mode)) {
			const char *src;

			/* skip the prefix, get the name actually
			   returned by the nested iterator */
			src = ent->name + prefix_len;
			while (*src == '/')
				++src;

			/* reconstruct base path relative file path */
			if (file_prefix == NULL) {
				extra = strdup(src);
			} else {
				size_t fxlen = strlen(file_prefix) + 1;
				size_t srclen = strlen(src) + 1;

				extra = malloc(fxlen + srclen);
				if (extra != NULL) {
					memcpy(extra, file_prefix, fxlen);
					memcpy(extra + fxlen, src, srclen);
					extra[fxlen - 1] = '/';
				}
			}

			if (extra == NULL) {
				free(ent);
				fputs("out-of-memory\n", stderr);
				return -1;
			}
		}

		n = fstree_add_generic(fs, ent, extra);
		free(extra);
		free(ent);

		if (n == NULL) {
			perror("creating tree node");
			return -1;
		}
	}

	return 0;
}

int glob_files(fstree_t *fs, const char *filename, size_t line_num,
	       const sqfs_dir_entry_t *ent,
	       const char *basepath, unsigned int glob_flags,
	       split_line_t *sep)
{
	bool first_type_flag = true;
	sqfs_dir_iterator_t *dir = NULL;
	const char *file_prefix = NULL;
	char *prefix = NULL;
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
	memset(&cfg, 0, sizeof(cfg));
	cfg.def_mtime = ent->mtime;
	cfg.def_uid = ent->uid;
	cfg.def_gid = ent->gid;
	cfg.def_mode = ent->mode;
	cfg.prefix = prefix;
	cfg.flags = glob_flags;

	while (sep->count != 0) {
		if (sep->args[0][0] != '-')
			break;

		if (!strcmp(sep->args[0], "--")) {
			split_line_remove_front(sep, 1);
			break;
		}

		if (set_scan_flag(sep->args[0], &cfg)) {
			split_line_remove_front(sep, 1);
		} else if (!strcmp(sep->args[0], "-type")) {
			if (sep->count < 2)
				goto fail_no_arg;

			if (!apply_type_flag(first_type_flag, sep->args[1],
					     &cfg.flags)) {
				goto fail_type;
			}

			first_type_flag = false;
			split_line_remove_front(sep, 2);
		} else if (!strcmp(sep->args[0], "-name")) {
			if (sep->count < 2)
				goto fail_no_arg;
			cfg.name_pattern = sep->args[1];
			split_line_remove_front(sep, 2);
		} else if (!strcmp(sep->args[0], "-path")) {
			if (sep->count < 2)
				goto fail_no_arg;
			cfg.name_pattern = sep->args[1];
			cfg.flags |= DIR_SCAN_MATCH_FULL_PATH;
			split_line_remove_front(sep, 2);
		} else {
			goto fail_unknown;
		}
	}

	/* do the scan */
	if (sep->count == 0) {
		dir = dir_tree_iterator_create(basepath, &cfg);
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

		file_prefix = sep->args[0];
	}

	if (dir == NULL)
		goto fail;

	ret = scan_directory(fs, dir, strlen(cfg.prefix), file_prefix);
	sqfs_drop(dir);

	free(prefix);
	return ret;
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
	free(prefix);
	return -1;
}
