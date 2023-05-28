/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static int add_generic(fstree_t *fs, const char *filename, size_t line_num,
		       const char *path, struct stat *sb, const char *extra)
{
	if (fstree_add_generic(fs, path, sb, extra) == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s: %s\n",
			filename, line_num, path, strerror(errno));
		return -1;
	}

	return 0;
}

static int add_device(fstree_t *fs, const char *filename, size_t line_num,
		      const char *path, struct stat *sb, const char *extra)
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
		sb->st_mode |= S_IFCHR;
	} else if (c == 'b' || c == 'B') {
		sb->st_mode |= S_IFBLK;
	} else {
		fprintf(stderr, "%s: " PRI_SZ ": unknown device type '%c'\n",
			filename, line_num, c);
		return -1;
	}

	sb->st_rdev = makedev(maj, min);
	return add_generic(fs, filename, line_num, path, sb, NULL);
}

static int add_file(fstree_t *fs, const char *filename, size_t line_num,
		    const char *path, struct stat *basic, const char *extra)
{
	if (extra == NULL || *extra == '\0')
		extra = path;

	return add_generic(fs, filename, line_num, path, basic, extra);
}

static int add_hard_link(fstree_t *fs, const char *filename, size_t line_num,
			 const char *path, struct stat *basic,
			 const char *extra)
{
	(void)basic;

	if (fstree_add_hard_link(fs, path, extra) == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s\n",
			filename, line_num, strerror(errno));
		return -1;
	}
	return 0;
}

static const struct callback_t {
	const char *keyword;
	unsigned int mode;
	bool need_extra;
	bool allow_root;
	int (*callback)(fstree_t *fs, const char *filename, size_t line_num,
			const char *path, struct stat *sb, const char *extra);
} file_list_hooks[] = {
	{ "dir", S_IFDIR, false, true, add_generic },
	{ "slink", S_IFLNK, true, false, add_generic },
	{ "link", 0, true, false, add_hard_link },
	{ "nod", 0, true,  false, add_device },
	{ "pipe", S_IFIFO, false, false, add_generic },
	{ "sock", S_IFSOCK, false, false, add_generic },
	{ "file", S_IFREG, false, false, add_file },
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
	bool is_glob = false;
	struct stat sb;
	char *path;

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
	memset(&sb, 0, sizeof(sb));
	sb.st_mtime = fs->defaults.mtime;
	sb.st_mode = mode | (is_glob ? 0 : cb->mode);
	sb.st_uid = uid;
	sb.st_gid = gid;

	if (is_glob) {
		return glob_files(fs, filename, line_num, path, &sb,
				  basepath, glob_flags, extra);
	}

	return cb->callback(fs, filename, line_num, path, &sb, extra);
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
