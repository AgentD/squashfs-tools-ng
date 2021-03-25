/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"

#include "fstree.h"
#include "fstream.h"
#include "compat.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <ctype.h>

struct glob_context {
	const char *filename;
	size_t line_num;

	struct stat *basic;
	unsigned int glob_flags;

	char *name_pattern;
};

enum {
	GLOB_MODE_FROM_SRC = 0x01,
	GLOB_UID_FROM_SRC = 0x02,
	GLOB_GID_FROM_SRC = 0x04,
	GLOB_FLAG_PATH = 0x08,
};

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

static int add_generic(fstree_t *fs, const char *filename, size_t line_num,
		       const char *path, struct stat *sb,
		       const char *basepath, unsigned int glob_flags,
		       const char *extra)
{
	(void)basepath;
	(void)glob_flags;

	if (fstree_add_generic(fs, path, sb, extra) == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s: %s\n",
			filename, line_num, path, strerror(errno));
		return -1;
	}

	return 0;
}

static int add_device(fstree_t *fs, const char *filename, size_t line_num,
		      const char *path, struct stat *sb, const char *basepath,
		      unsigned int glob_flags, const char *extra)
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
	return add_generic(fs, filename, line_num, path, sb, basepath,
			   glob_flags, NULL);
}

static int add_file(fstree_t *fs, const char *filename, size_t line_num,
		    const char *path, struct stat *basic, const char *basepath,
		    unsigned int glob_flags, const char *extra)
{
	if (extra == NULL || *extra == '\0')
		extra = path;

	return add_generic(fs, filename, line_num, path, basic,
			   basepath, glob_flags, extra);
}

static int add_hard_link(fstree_t *fs, const char *filename, size_t line_num,
			 const char *path, struct stat *basic,
			 const char *basepath, unsigned int glob_flags,
			 const char *extra)
{
	(void)basepath;
	(void)glob_flags;
	(void)basic;

	if (fstree_add_hard_link(fs, path, extra) == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s\n",
			filename, line_num, strerror(errno));
		return -1;
	}
	return 0;
}

static int glob_node_callback(void *user, fstree_t *fs, tree_node_t *node)
{
	struct glob_context *ctx = user;
	char *path;
	int ret;
	(void)fs;

	if (!(ctx->glob_flags & GLOB_MODE_FROM_SRC)) {
		node->mode &= ~(07777);
		node->mode |= ctx->basic->st_mode & 07777;
	}

	if (!(ctx->glob_flags & GLOB_UID_FROM_SRC))
		node->uid = ctx->basic->st_uid;

	if (!(ctx->glob_flags & GLOB_GID_FROM_SRC))
		node->gid = ctx->basic->st_gid;

	if (ctx->name_pattern != NULL) {
		if (ctx->glob_flags & GLOB_FLAG_PATH) {
			path = fstree_get_path(node);
			if (path == NULL) {
				fprintf(stderr, "%s: " PRI_SZ ": %s\n",
					ctx->filename, ctx->line_num,
					strerror(errno));
				return -1;
			}

			ret = canonicalize_name(path);
			assert(ret == 0);

			ret = fnmatch(ctx->name_pattern, path, FNM_PATHNAME);
			free(path);
		} else {
			ret = fnmatch(ctx->name_pattern, node->name, 0);
		}

		if (ret != 0)
			return 1;
	}

	return 0;
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

static int glob_files(fstree_t *fs, const char *filename, size_t line_num,
		      const char *path, struct stat *basic,
		      const char *basepath, unsigned int glob_flags,
		      const char *extra)
{
	unsigned int scan_flags = 0, all_flags;
	struct glob_context ctx;
	bool first_clear_flag;
	size_t i, count, len;
	tree_node_t *root;
	int ret;

	memset(&ctx, 0, sizeof(ctx));
	ctx.filename = filename;
	ctx.line_num = line_num;
	ctx.basic = basic;
	ctx.glob_flags = glob_flags;

	/* fetch the actual target node */
	root = fstree_get_node_by_path(fs, fs->root, path, true, false);
	if (root == NULL) {
		fprintf(stderr, "%s: " PRI_SZ ": %s: %s\n",
			filename, line_num, path, strerror(errno));
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

			free(ctx.name_pattern);
			ctx.name_pattern = strndup(extra, len);
			extra += len;

			while (isspace(*extra))
				++extra;

			quote_remove(ctx.name_pattern);
			continue;
		}

		if (strncmp(extra, "-path", 5) == 0 && isspace(extra[5])) {
			for (extra += 5; isspace(*extra); ++extra)
				;

			len = name_string_length(extra);

			free(ctx.name_pattern);
			ctx.name_pattern = strndup(extra, len);
			extra += len;

			while (isspace(*extra))
				++extra;

			quote_remove(ctx.name_pattern);
			ctx.glob_flags |= GLOB_FLAG_PATH;
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
			free(ctx.name_pattern);
			return -1;
		} else {
			break;
		}
	}

	if (extra != NULL && *extra == '\0')
		extra = NULL;

	/* do the scan */
	if (basepath == NULL) {
		if (extra == NULL) {
			ret = fstree_from_dir(fs, root, ".", glob_node_callback,
					      &ctx, scan_flags);
		} else {
			ret = fstree_from_dir(fs, root, extra,
					      glob_node_callback,
					      &ctx, scan_flags);
		}
	} else {
		ret = fstree_from_subdir(fs, root, basepath, extra,
					 glob_node_callback, &ctx,
					 scan_flags);
	}

	free(ctx.name_pattern);
	return ret;
}

static const struct callback_t {
	const char *keyword;
	unsigned int mode;
	bool need_extra;
	bool is_glob;
	bool allow_root;
	int (*callback)(fstree_t *fs, const char *filename, size_t line_num,
			const char *path, struct stat *sb,
			const char *basepath, unsigned int glob_flags,
			const char *extra);
} file_list_hooks[] = {
	{ "dir", S_IFDIR, false, false, true, add_generic },
	{ "slink", S_IFLNK, true, false, false, add_generic },
	{ "link", 0, true, false, false, add_hard_link },
	{ "nod", 0, true, false, false, add_device },
	{ "pipe", S_IFIFO, false, false, false, add_generic },
	{ "sock", S_IFSOCK, false, false, false, add_generic },
	{ "file", S_IFREG, false, false, false, add_file },
	{ "glob", 0, false, true, true, glob_files },
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

	if (cb == NULL)
		goto fail_kw;

	if ((line = read_str(line, &path)) == NULL)
		goto fail_ent;

	if (canonicalize_name(path))
		goto fail_ent;

	if (*path == '\0' && !cb->allow_root)
		goto fail_root;

	if (cb->is_glob && *line == '*') {
		++line;
		mode = 0;
		glob_flags |= GLOB_MODE_FROM_SRC;
	} else {
		if ((line = read_u32(line, &mode, 8)) == NULL || mode > 07777)
			goto fail_mode;
	}

	if ((line = skip_space(line)) == NULL)
		goto fail_ent;

	if (cb->is_glob && *line == '*') {
		++line;
		uid = 0;
		glob_flags |= GLOB_UID_FROM_SRC;
	} else {
		if ((line = read_u32(line, &uid, 10)) == NULL)
			goto fail_uid_gid;
	}

	if ((line = skip_space(line)) == NULL)
		goto fail_ent;

	if (cb->is_glob && *line == '*') {
		++line;
		gid = 0;
		glob_flags |= GLOB_GID_FROM_SRC;
	} else {
		if ((line = read_u32(line, &gid, 10)) == NULL)
			goto fail_uid_gid;
	}

	if ((line = skip_space(line)) != NULL && *line != '\0')
		extra = line;

	if (cb->need_extra && extra == NULL)
		goto fail_no_extra;

	/* forward to callback */
	memset(&sb, 0, sizeof(sb));
	sb.st_mtime = fs->defaults.st_mtime;
	sb.st_mode = mode | cb->mode;
	sb.st_uid = uid;
	sb.st_gid = gid;

	return cb->callback(fs, filename, line_num, path,
			    &sb, basepath, glob_flags, extra);
fail_root:
	fprintf(stderr, "%s: " PRI_SZ ": cannot use / as argument for %s.\n",
		filename, line_num, cb->keyword);
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

int fstree_from_file(fstree_t *fs, const char *filename, const char *basepath)
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
			if (handle_line(fs, filename, line_num,
					line, basepath)) {
				goto fail_line;
			}
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
