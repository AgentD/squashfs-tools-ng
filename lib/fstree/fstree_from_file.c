/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "fstream.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

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
			 const char *path, struct stat *basic, const char *extra)
{
	(void)basic;

	if (fstree_add_hard_link(fs, path, extra) == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s\n",
			filename, line_num, strerror(errno));
		return -1;
	}
	return 0;
}

static const struct {
	const char *keyword;
	unsigned int mode;
	bool need_extra;
	int (*callback)(fstree_t *fs, const char *filename, size_t line_num,
			const char *path, struct stat *sb, const char *extra);
} file_list_hooks[] = {
	{ "dir", S_IFDIR, false, add_generic },
	{ "slink", S_IFLNK, true, add_generic },
	{ "link", 0, true, add_hard_link },
	{ "nod", 0, true, add_device },
	{ "pipe", S_IFIFO, false, add_generic },
	{ "sock", S_IFSOCK, false, add_generic },
	{ "file", S_IFREG, false, add_file },
};

#define NUM_HOOKS (sizeof(file_list_hooks) / sizeof(file_list_hooks[0]))

static int handle_line(fstree_t *fs, const char *filename,
		       size_t line_num, char *line)
{
	const char *extra = NULL, *msg = NULL;
	char keyword[16], *path, *ptr;
	unsigned int x;
	struct stat sb;
	size_t i;

	memset(&sb, 0, sizeof(sb));
	sb.st_mtime = fs->defaults.st_mtime;

	/* isolate keyword */
	for (i = 0; isalpha(line[i]); ++i)
		;

	if (i >= sizeof(keyword) || i == 0 || !isspace(line[i]))
		goto fail_ent;

	memcpy(keyword, line, i);
	keyword[i] = '\0';

	while (isspace(line[i]))
		++i;

	/* isolate path */
	path = line + i;

	if (*path == '"') {
		ptr = path;
		++i;

		while (line[i] != '\0' && line[i] != '"') {
			if (line[i] == '\\' &&
			    (line[i + 1] == '"' || line[i + 1] == '\\')) {
				*(ptr++) = line[i + 1];
				i += 2;
			} else {
				*(ptr++) = line[i++];
			}
		}

		if (line[i] != '"' || !isspace(line[i + 1]))
			goto fail_ent;

		*ptr = '\0';
		++i;
	} else {
		while (line[i] != '\0' && !isspace(line[i]))
			++i;

		if (!isspace(line[i]))
			goto fail_ent;

		line[i++] = '\0';
	}

	while (isspace(line[i]))
		++i;

	if (canonicalize_name(path) || *path == '\0')
		goto fail_ent;

	/* mode */
	if (!isdigit(line[i]))
		goto fail_mode;

	for (; isdigit(line[i]); ++i) {
		if (line[i] > '7')
			goto fail_mode;

		sb.st_mode = (sb.st_mode << 3) | (line[i] - '0');

		if (sb.st_mode > 07777)
			goto fail_mode_bits;
	}

	if (!isspace(line[i]))
		goto fail_ent;

	while (isspace(line[i]))
		++i;

	/* uid */
	if (!isdigit(line[i]))
		goto fail_uid_gid;

	for (; isdigit(line[i]); ++i) {
		x = line[i] - '0';

		if (sb.st_uid > (0xFFFFFFFF - x) / 10)
			goto fail_ent;

		sb.st_uid = sb.st_uid * 10 + x;
	}

	if (!isspace(line[i]))
		goto fail_ent;

	while (isspace(line[i]))
		++i;

	/* gid */
	if (!isdigit(line[i]))
		goto fail_uid_gid;

	for (; isdigit(line[i]); ++i) {
		x = line[i] - '0';

		if (sb.st_gid > (0xFFFFFFFF - x) / 10)
			goto fail_ent;

		sb.st_gid = sb.st_gid * 10 + x;
	}

	/* extra */
	if (isspace(line[i])) {
		while (isspace(line[i]))
			++i;

		if (line[i] != '\0')
			extra = line + i;
	}

	/* forward to callback */
	for (i = 0; i < NUM_HOOKS; ++i) {
		if (strcmp(file_list_hooks[i].keyword, keyword) == 0) {
			if (file_list_hooks[i].need_extra && extra == NULL)
				goto fail_no_extra;

			sb.st_mode |= file_list_hooks[i].mode;

			return file_list_hooks[i].callback(fs, filename,
							   line_num, path,
							   &sb, extra);
		}
	}

	fprintf(stderr, "%s: " PRI_SZ ": unknown entry type '%s'.\n", filename,
		line_num, keyword);
	return -1;
fail_no_extra:
	fprintf(stderr, "%s: " PRI_SZ ": missing argument for %s.\n",
		filename, line_num, keyword);
	return -1;
fail_uid_gid:
	msg = "uid & gid must be decimal numbers";
	goto out_desc;
fail_mode:
	msg = "mode must be an octal number";
	goto out_desc;
fail_mode_bits:
	msg = "you can only set the permission bits in the mode";
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

int fstree_from_file(fstree_t *fs, const char *filename)
{
	size_t line_num = 1;
	istream_t *fp;
	char *line;
	int ret;

	fp = istream_open_file(filename);
	if (fp == NULL)
		return -1;

	for (;;) {
		ret = istream_get_line(fp, &line, &line_num,
				       ISTREAM_LINE_LTRIM | ISTREAM_LINE_SKIP_EMPTY);
		if (ret < 0)
			return -1;
		if (ret > 0)
			break;

		if (line[0] != '#') {
			if (handle_line(fs, filename, line_num, line))
				goto fail_line;
		}

		free(line);
		++line_num;
	}

	sqfs_destroy(fp);
	return 0;
fail_line:
	free(line);
	sqfs_destroy(fp);
	return -1;
}
