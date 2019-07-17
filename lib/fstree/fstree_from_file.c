/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"
#include "util.h"

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static int add_generic(fstree_t *fs, const char *filename, size_t line_num,
		       const char *path, struct stat *sb, const char *extra)
{
	if (fstree_add_generic(fs, path, sb, extra) == NULL) {
		fprintf(stderr, "%s: %zu: %s: %s\n",
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
		fprintf(stderr, "%s: %zu: expected '<c|b> major minor'\n",
			filename, line_num);
		return -1;
	}

	if (c == 'c' || c == 'C') {
		sb->st_mode |= S_IFCHR;
	} else if (c == 'b' || c == 'B') {
		sb->st_mode |= S_IFBLK;
	} else {
		fprintf(stderr, "%s: %zu: unknown device type '%c'\n",
			filename, line_num, c);
		return -1;
	}

	sb->st_rdev = makedev(maj, min);
	return add_generic(fs, filename, line_num, path, sb, NULL);
}

static int add_file(fstree_t *fs, const char *filename, size_t line_num,
		    const char *path, struct stat *basic, const char *extra)
{
	struct stat sb;

	if (extra == NULL || *extra == '\0')
		extra = path;

	if (stat(extra, &sb) != 0) {
		fprintf(stderr, "%s: %zu: stat %s: %s\n", filename, line_num,
			extra, strerror(errno));
		return -1;
	}

	sb.st_uid = basic->st_uid;
	sb.st_gid = basic->st_gid;
	sb.st_mode = basic->st_mode;

	return add_generic(fs, filename, line_num, path, &sb, extra);
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
	{ "nod", 0, true, add_device },
	{ "pipe", S_IFIFO, false, add_generic },
	{ "sock", S_IFSOCK, false, add_generic },
	{ "file", S_IFREG, false, add_file },
};

#define NUM_HOOKS (sizeof(file_list_hooks) / sizeof(file_list_hooks[0]))

static void trim_line(char *line)
{
	size_t i;

	for (i = 0; isspace(line[i]); ++i)
		;

	if (line[i] == '#') {
		line[0] = '\0';
		return;
	}

	if (i > 0)
		memmove(line, line + i, strlen(line + i) + 1);

	i = strlen(line);
	while (i > 0 && isspace(line[i - 1]))
		--i;

	line[i] = '\0';
}

static int handle_line(fstree_t *fs, const char *filename,
		       size_t line_num, char *line)
{
	const char *extra = NULL, *msg = NULL;
	char keyword[16], *path, *ptr;
	unsigned int x;
	struct stat sb;
	size_t i;

	memset(&sb, 0, sizeof(sb));

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

	fprintf(stderr, "%s: %zu: unknown entry type '%s'.\n", filename,
		line_num, keyword);
	return -1;
fail_no_extra:
	fprintf(stderr, "%s: %zu: missing argument for %s.\n",
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
	fprintf(stderr, "%s: %zu: %s.\n", filename, line_num, msg);
	fputs("expected: <type> <path> <mode> <uid> <gid> [<extra>]\n",
	      stderr);
	return -1;
}

int fstree_from_file(fstree_t *fs, const char *filename, FILE *fp)
{
	size_t n, line_num = 0;
	ssize_t ret;
	char *line;

	for (;;) {
		line = NULL;
		n = 0;
		errno = 0;

		ret = getline(&line, &n, fp);
		++line_num;

		if (ret < 0) {
			if (errno == 0) {
				free(line);
				break;
			}

			perror(filename);
			goto fail_line;
		}

		trim_line(line);

		if (line[0] == '\0') {
			free(line);
			continue;
		}

		if (handle_line(fs, filename, line_num, line))
			goto fail_line;

		free(line);
	}
	return 0;
fail_line:
	free(line);
	return -1;
}
