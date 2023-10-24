/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_file.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "mkfs.h"

static int add_generic(fstree_t *fs, const char *filename, size_t line_num,
		       sqfs_dir_entry_t *ent, split_line_t *line)
{
	const char *msg = NULL, *arg = line->count > 0 ? line->args[0] : NULL;

	if (line->count > 1) {
		msg = "too many arguments";
		goto fail;
	}

	if (fstree_add_generic(fs, ent, arg) == NULL) {
		msg = strerror(errno);
		goto fail;
	}

	return 0;
fail:
	fprintf(stderr, "%s: " PRI_SZ ": %s: %s\n",
		filename, line_num, ent->name, msg);
	return -1;
}

static int add_device(fstree_t *fs, const char *filename, size_t line_num,
		      sqfs_dir_entry_t *ent, split_line_t *line)
{
	sqfs_u64 maj, min;

	if (line->count != 3)
		goto fail_args;

	if (!strcmp(line->args[0], "c") || !strcmp(line->args[0], "C")) {
		ent->mode |= S_IFCHR;
	} else if (!strcmp(line->args[0], "b") || !strcmp(line->args[0], "B")) {
		ent->mode |= S_IFBLK;
	} else {
		goto fail_type;
	}

	if (parse_uint(line->args[1], -1, NULL, 0, 0x0FFFFFFFF, &maj))
		goto fail_num;
	if (parse_uint(line->args[2], -1, NULL, 0, 0x0FFFFFFFF, &min))
		goto fail_num;

	ent->rdev = makedev(maj, min);

	split_line_remove_front(line, 3);
	return add_generic(fs, filename, line_num, ent, line);
fail_args:
	fprintf(stderr, "%s: " PRI_SZ ": wrong number of arguments.\n",
		filename, line_num);
	goto fail_generic;
fail_type:
	fprintf(stderr, "%s: " PRI_SZ ": unknown device type `%s`\n",
		filename, line_num, line->args[0]);
	goto fail_generic;
fail_num:
	fprintf(stderr, "%s: " PRI_SZ ": error parsing device number\n",
		filename, line_num);
	goto fail_generic;
fail_generic:
	fputs("expected syntax: `nod <c|b> <major> <minor>`\n", stderr);
	return -1;
}

static int add_file(fstree_t *fs, const char *filename, size_t line_num,
		    sqfs_dir_entry_t *ent, split_line_t *line)
{
	if (line->count == 0)
		line->args[line->count++] = ent->name;

	return add_generic(fs, filename, line_num, ent, line);
}

static const struct callback_t {
	const char *keyword;
	unsigned int mode;
	unsigned int flags;
	bool need_extra;
	bool allow_root;
	int (*callback)(fstree_t *fs, const char *filename, size_t line_num,
			sqfs_dir_entry_t *ent, split_line_t *line);
} file_list_hooks[] = {
	{ "dir", S_IFDIR, 0, false, true, add_generic },
	{ "slink", S_IFLNK, 0, true, false, add_generic },
	{ "link", S_IFLNK, SQFS_DIR_ENTRY_FLAG_HARD_LINK, true, false, add_generic },
	{ "nod", 0, 0, true,  false, add_device },
	{ "pipe", S_IFIFO, 0, false, false, add_generic },
	{ "sock", S_IFSOCK, 0, false, false, add_generic },
	{ "file", S_IFREG, 0, false, false, add_file },
};

#define NUM_HOOKS (sizeof(file_list_hooks) / sizeof(file_list_hooks[0]))

static int handle_line(fstree_t *fs, const char *filename, size_t line_num,
		       split_line_t *line, const options_t *opt)
{
	sqfs_u64 uid = 0, gid = 0, mode = 0;
	const struct callback_t *cb = NULL;
	unsigned int glob_flags = 0;
	sqfs_dir_entry_t *ent = NULL;
	const char *msg = NULL;
	bool is_glob = false;
	char *path;
	int ret;

	if (line->count < 5)
		goto fail_ent;

	for (size_t i = 0; i < NUM_HOOKS; ++i) {
		if (!strcmp(file_list_hooks[i].keyword, line->args[0])) {
			cb = file_list_hooks + i;
			break;
		}
	}

	if (cb == NULL) {
		if (!strcmp("glob", line->args[0])) {
			is_glob = true;
		} else {
			goto fail_kw;
		}
	}

	path = line->args[1];
	if (canonicalize_name(path))
		goto fail_ent;

	if (*path == '\0' && !(is_glob || cb->allow_root))
		goto fail_root;

	if (is_glob && !strcmp(line->args[2], "*")) {
		glob_flags |= DIR_SCAN_KEEP_MODE;
	} else {
		if (parse_uint_oct(line->args[2], -1, NULL, 0, 07777, &mode))
			goto fail_mode;
	}

	if (is_glob && !strcmp(line->args[3], "*")) {
		glob_flags |= (opt->dirscan_flags & DIR_SCAN_KEEP_UID);
	} else {
		if (parse_uint(line->args[3], -1, NULL, 0, 0x0FFFFFFFF, &uid))
			goto fail_uid_gid;
	}

	if (!(opt->dirscan_flags & DIR_SCAN_KEEP_UID))
		uid = opt->force_uid_value;

	if (is_glob && !strcmp(line->args[4], "*")) {
		glob_flags |= (opt->dirscan_flags & DIR_SCAN_KEEP_GID);
	} else {
		if (parse_uint(line->args[4], -1, NULL, 0, 0x0FFFFFFFF, &gid))
			goto fail_uid_gid;
	}

	if (!(opt->dirscan_flags & DIR_SCAN_KEEP_GID))
		gid = opt->force_gid_value;

	if (!is_glob && cb->need_extra && line->count <= 5)
		goto fail_no_extra;

	split_line_remove_front(line, 5);

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
				 opt->packdir, glob_flags, line);
	} else {
		ret = cb->callback(fs, filename, line_num, ent, line);
	}

	free(ent);
	return ret;
fail_alloc:
	fprintf(stderr, "%s: " PRI_SZ ": out of memory\n",
		filename, line_num);
	return -1;
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

int fstree_from_file_stream(fstree_t *fs, sqfs_istream_t *fp,
			    const options_t *opt)
{
	const char *filename;
	size_t line_num = 1;
	split_line_t *sep;
	char *line;
	int ret;

	filename = fp->get_filename(fp);

	for (;;) {
		ret = istream_get_line(fp, &line, &line_num,
				       ISTREAM_LINE_LTRIM | ISTREAM_LINE_SKIP_EMPTY);
		if (ret < 0)
			return -1;
		if (ret > 0)
			break;

		if (line[0] == '#') {
			free(line);
			++line_num;
			continue;
		}

		switch (split_line(line, strlen(line), " \t", &sep)) {
		case SPLIT_LINE_OK:              break;
		case SPLIT_LINE_ALLOC:           goto fail_alloc;
		case SPLIT_LINE_UNMATCHED_QUOTE: goto fail_quote;
		case SPLIT_LINE_ESCAPE:          goto fail_esc;
		default:                         goto fail_split;
		}

		ret = handle_line(fs, filename, line_num, sep, opt);
		free(sep);
		free(line);
		++line_num;

		if (ret)
			return -1;
	}

	return 0;
fail_alloc:
	fprintf(stderr, "%s: " PRI_SZ ": out of memory\n", filename, line_num);
	goto fail_line;
fail_quote:
	fprintf(stderr, "%s: " PRI_SZ ": missing `\"`.\n", filename, line_num);
	goto fail_line;
fail_esc:
	fprintf(stderr, "%s: " PRI_SZ ": broken escape sequence.\n",
		filename, line_num);
	goto fail_line;
fail_split:
	fprintf(stderr, "[BUG] %s: " PRI_SZ ": unknown error parsing line.\n",
		filename, line_num);
	goto fail_line;
fail_line:
	free(line);
	return -1;
}

int fstree_from_file(fstree_t *fs, const char *filename, const options_t *opt)
{
	sqfs_istream_t *fp;
	int ret;

	ret = sqfs_istream_open_file(&fp, filename, 0);
	if (ret) {
		sqfs_perror(filename, NULL, ret);
		return -1;
	}

	ret = fstree_from_file_stream(fs, fp, opt);

	sqfs_drop(fp);
	return ret;
}
