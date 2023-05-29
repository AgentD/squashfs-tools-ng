/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static int add_generic(fstree_t *fs, const char *filename, size_t line_num,
		       dir_entry_t *ent, const char *extra)
{
	if (fstree_add_generic(fs, ent, extra) == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s: %s\n",
			filename, line_num, ent->name, strerror(errno));
		return -1;
	}

	return 0;
}

static int add_device(fstree_t *fs, const char *filename, size_t line_num,
		      dir_entry_t *ent, const char *extra)
{
	unsigned int maj, min;
	char c;

	if (sscanf(extra, "%c %u %u", &c, &maj, &min) != 3) {
		fprintf(stderr, "%s: " PRI_SZ ": "
			"expected '<c|b> major minor'\n",
			filename, line_num);
		return -1;
	}

	if (c == 'c' || c == 'C') {
		ent->mode |= S_IFCHR;
	} else if (c == 'b' || c == 'B') {
		ent->mode |= S_IFBLK;
	} else {
		fprintf(stderr, "%s: " PRI_SZ ": unknown device type '%c'\n",
			filename, line_num, c);
		return -1;
	}

	ent->rdev = makedev(maj, min);
	return add_generic(fs, filename, line_num, ent, NULL);
}

static int add_file(fstree_t *fs, const char *filename, size_t line_num,
		    dir_entry_t *ent, const char *extra)
{
	if (extra == NULL || *extra == '\0')
		extra = ent->name;

	return add_generic(fs, filename, line_num, ent, extra);
}

static const struct callback_t {
	const char *keyword;
	unsigned int mode;
	unsigned int flags;
	bool need_extra;
	bool allow_root;
	int (*callback)(fstree_t *fs, const char *filename, size_t line_num,
			dir_entry_t *ent, const char *extra);
} file_list_hooks[] = {
	{ "dir", S_IFDIR, 0, false, true, add_generic },
	{ "slink", S_IFLNK, 0, true, false, add_generic },
	{ "link", S_IFLNK, DIR_ENTRY_FLAG_HARD_LINK, true, false, add_generic },
	{ "nod", 0, 0, true,  false, add_device },
	{ "pipe", S_IFIFO, 0, false, false, add_generic },
	{ "sock", S_IFSOCK, 0, false, false, add_generic },
	{ "file", S_IFREG, 0, false, false, add_file },
};

#define NUM_HOOKS (sizeof(file_list_hooks) / sizeof(file_list_hooks[0]))

static char *skip_space(char *str)
{
	if (!isspace(*str))
		return NULL;
	while (isspace(*str))
		++str;
	return str;
}

static char *read_u32(char *str, sqfs_u32 *out, sqfs_u32 base)
{
	*out = 0;

	if (!isdigit(*str))
		return NULL;

	while (isdigit(*str)) {
		sqfs_u32 x = *(str++) - '0';

		if (x >= base || (*out) > (0xFFFFFFFF - x) / base)
			return NULL;

		(*out) = (*out) * base + x;
	}

	return str;
}

static char *read_str(char *str, char **out)
{
	*out = str;

	if (*str == '"') {
		char *ptr = str++;

		while (*str != '\0' && *str != '"') {
			if (str[0] == '\\' &&
			    (str[1] == '"' || str[1] == '\\')) {
				*(ptr++) = str[1];
				str += 2;
			} else {
				*(ptr++) = *(str++);
			}
		}

		if (str[0] != '"' || !isspace(str[1]))
			return NULL;

		*ptr = '\0';
		++str;
	} else {
		while (*str != '\0' && !isspace(*str))
			++str;

		if (!isspace(*str))
			return NULL;

		*(str++) = '\0';
	}

	while (isspace(*str))
		++str;

	return str;
}

static int handle_line(fstree_t *fs, const char *filename,
		       size_t line_num, char *line,
		       const char *basepath)
{
	const char *extra = NULL, *msg = NULL;
	const struct callback_t *cb = NULL;
	unsigned int glob_flags = 0;
	sqfs_u32 uid, gid, mode;
	dir_entry_t *ent = NULL;
	bool is_glob = false;
	char *path;
	int ret;

	for (size_t i = 0; i < NUM_HOOKS; ++i) {
		size_t len = strlen(file_list_hooks[i].keyword);
		if (strncmp(file_list_hooks[i].keyword, line, len) != 0)
			continue;

		if (isspace(line[len])) {
			cb = file_list_hooks + i;
			line = skip_space(line + len);
			break;
		}
	}

	if (cb == NULL) {
		if (strncmp("glob", line, 4) == 0 && isspace(line[4])) {
			line = skip_space(line + 4);
			is_glob = true;
		} else {
			goto fail_kw;
		}
	}

	if ((line = read_str(line, &path)) == NULL)
		goto fail_ent;

	if (canonicalize_name(path))
		goto fail_ent;

	if (*path == '\0' && !(is_glob || cb->allow_root))
		goto fail_root;

	if (is_glob && *line == '*') {
		++line;
		mode = 0;
		glob_flags |= DIR_SCAN_KEEP_MODE;
	} else {
		if ((line = read_u32(line, &mode, 8)) == NULL || mode > 07777)
			goto fail_mode;
	}

	if ((line = skip_space(line)) == NULL)
		goto fail_ent;

	if (is_glob && *line == '*') {
		++line;
		uid = 0;
		glob_flags |= DIR_SCAN_KEEP_UID;
	} else {
		if ((line = read_u32(line, &uid, 10)) == NULL)
			goto fail_uid_gid;
	}

	if ((line = skip_space(line)) == NULL)
		goto fail_ent;

	if (is_glob && *line == '*') {
		++line;
		gid = 0;
		glob_flags |= DIR_SCAN_KEEP_GID;
	} else {
		if ((line = read_u32(line, &gid, 10)) == NULL)
			goto fail_uid_gid;
	}

	if ((line = skip_space(line)) != NULL && *line != '\0')
		extra = line;

	if (!is_glob && cb->need_extra && extra == NULL)
		goto fail_no_extra;

	/* forward to callback */
	ent = alloc_flex(sizeof(*ent), 1, strlen(path) + 1);
	if (ent == NULL)
		goto fail_alloc;

	strcpy(ent->name, path);
	ent->mtime = fs->defaults.mtime;
	ent->mode = mode | (is_glob ? 0 : cb->mode);
	ent->uid = uid;
	ent->gid = gid;

	if (is_glob) {
		ret = glob_files(fs, filename, line_num, ent,
				 basepath, glob_flags, extra);
	} else {
		ret = cb->callback(fs, filename, line_num, ent, extra);
	}

	free(ent);
	return ret;
fail_alloc:
	fprintf(stderr, "%s: " PRI_SZ ": out of memory\n",
		filename, line_num);
	return -1;
fail_root:
	fprintf(stderr, "%s: " PRI_SZ ": cannot use / as argument for %s.\n",
		filename, line_num, is_glob ? "glob" : cb->keyword);
	return -1;
fail_no_extra:
	fprintf(stderr, "%s: " PRI_SZ ": missing argument for %s.\n",
		filename, line_num, cb->keyword);
	return -1;
fail_uid_gid:
	msg = "uid & gid must be decimal numbers < 2^32";
	goto out_desc;
fail_mode:
	msg = "mode must be an octal number <= 07777";
	goto out_desc;
fail_kw:
	msg = "unknown entry type";
	goto out_desc;
fail_ent:
	msg = "error in entry description";
	goto out_desc;
out_desc:
	fprintf(stderr, "%s: " PRI_SZ ": %s.\n", filename, line_num, msg);
	fputs("expected: <type> <path> <mode> <uid> <gid> [<extra>]\n",
	      stderr);
	return -1;
}

int fstree_from_file_stream(fstree_t *fs, istream_t *fp, const char *basepath)
{
	const char *filename;
	size_t line_num = 1;
	char *line;
	int ret;

	filename = istream_get_filename(fp);

	for (;;) {
		ret = istream_get_line(fp, &line, &line_num,
				       ISTREAM_LINE_LTRIM | ISTREAM_LINE_SKIP_EMPTY);
		if (ret < 0)
			return -1;
		if (ret > 0)
			break;

		if (line[0] != '#') {
			if (handle_line(fs, filename, line_num,
					line, basepath)) {
				goto fail_line;
			}
		}

		free(line);
		++line_num;
	}

	return 0;
fail_line:
	free(line);
	return -1;
}

int fstree_from_file(fstree_t *fs, const char *filename, const char *basepath)
{
	istream_t *fp;
	int ret;

	fp = istream_open_file(filename);
	if (fp == NULL)
		return -1;

	ret = fstree_from_file_stream(fs, fp, basepath);

	sqfs_drop(fp);
	return ret;
}
