/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "fstree.h"
#include "util.h"

#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

static int add_dir(fstree_t *fs, const char *filename, size_t line_num,
		   const char *path, uint16_t mode, uint32_t uid,
		   uint32_t gid, const char *extra)
{
	if (extra != NULL && *extra != '\0') {
		fprintf(stderr, "%s: %zu: WARNING: ignoring extra arguments\n",
			filename, line_num);
	}

	if (fstree_add(fs, path, S_IFDIR | mode, uid, gid, 0) == NULL) {
		fprintf(stderr, "%s: %zu: mkdir -p %s: %s\n",
			filename, line_num, path, strerror(errno));
		return -1;
	}

	return 0;
}

static int add_slink(fstree_t *fs, const char *filename, size_t line_num,
		     const char *path, uint16_t mode, uint32_t uid,
		     uint32_t gid, const char *extra)
{
	tree_node_t *node;
	(void)mode;

	if (extra == NULL || *extra == '\0') {
		fprintf(stderr, "%s: %zu: missing symlink target\n",
			filename, line_num);
		return -1;
	}

	node = fstree_add(fs, path, S_IFLNK | 0777, uid, gid,
			  strlen(extra) + 1);

	if (node == NULL) {
		fprintf(stderr, "%s: %zu: ln -s %s %s\n",
			filename, line_num, extra, path);
		return -1;
	}

	strcpy(node->data.slink_target, extra);
	return 0;
}

static int add_device(fstree_t *fs, const char *filename, size_t line_num,
		      const char *path, uint16_t mode, uint32_t uid,
		      uint32_t gid, const char *extra)
{
	unsigned int maj = 0, min = 0;
	tree_node_t *node;

	if (extra == NULL || *extra == '\0') {
		fprintf(stderr, "%s: %zu: missing device type\n",
			filename, line_num);
		return -1;
	}

	if ((*extra == 'c' || *extra == 'C') && isspace(extra[1])) {
		mode |= S_IFCHR;
	} else if ((*extra == 'b' || *extra == 'B') && isspace(extra[1])) {
		mode |= S_IFBLK;
	} else {
		fprintf(stderr, "%s: %zu: unsupported device type",
			filename, line_num);
		return -1;
	}

	++extra;
	while (isspace(*extra))
		++extra;

	if (!isdigit(*extra))
		goto fail_devno;

	while (isdigit(*extra))
		maj = maj * 10 + *(extra++) - '0';

	if (!isspace(*extra))
		goto fail_devno;
	while (isspace(*extra))
		++extra;

	if (!isdigit(*extra))
		goto fail_devno;

	while (isdigit(*extra))
		min = min * 10 + *(extra++) - '0';

	while (isspace(*extra))
		++extra;

	if (*extra != '\0') {
		fprintf(stderr, "%s: %zu: WARNING: ignoring extra arguments\n",
			filename, line_num);
	}

	node = fstree_add(fs, path, mode, uid, gid, 0);
	if (node == NULL) {
		fprintf(stderr, "%s: %zu: mknod %s %c %u %u: %s\n",
			filename, line_num, path, S_ISCHR(mode) ? 'c' : 'b',
			maj, min, strerror(errno));
		return -1;
	}

	node->data.devno = makedev(maj, min);
	return 0;
fail_devno:
	fprintf(stderr, "%s: %zu: error in device number format\n",
		filename, line_num);
	return -1;
}

static int add_pipe(fstree_t *fs, const char *filename, size_t line_num,
		    const char *path, uint16_t mode, uint32_t uid,
		    uint32_t gid, const char *extra)
{
	if (extra != NULL && *extra != '\0') {
		fprintf(stderr, "%s: %zu: WARNING: ignoring extra arguments\n",
			filename, line_num);
	}

	if (fstree_add(fs, path, S_IFIFO | mode, uid, gid, 0) == NULL) {
		fprintf(stderr, "%s: %zu: mkfifo %s: %s\n",
			filename, line_num, path, strerror(errno));
		return -1;
	}

	return 0;
}

static int add_socket(fstree_t *fs, const char *filename, size_t line_num,
		      const char *path, uint16_t mode, uint32_t uid,
		      uint32_t gid, const char *extra)
{
	if (extra != NULL && *extra != '\0') {
		fprintf(stderr, "%s: %zu: WARNING: ignoring extra arguments\n",
			filename, line_num);
	}

	if (fstree_add(fs, path, S_IFSOCK | mode, uid, gid, 0) == NULL) {
		fprintf(stderr, "%s: %zu: creating Unix socket %s: %s\n",
			filename, line_num, path, strerror(errno));
		return -1;
	}

	return 0;
}

static int add_file(fstree_t *fs, const char *filename, size_t line_num,
		    const char *path, uint16_t mode, uint32_t uid,
		    uint32_t gid, const char *extra)
{
	tree_node_t *node;
	struct stat sb;

	if (extra == NULL || *extra == '\0')
		extra = path;

	if (stat(extra, &sb) != 0) {
		fprintf(stderr, "%s: %zu: stat %s: %s\n", filename, line_num,
			extra, strerror(errno));
		return -1;
	}

	node = fstree_add_file(fs, path, mode, uid, gid, sb.st_size, extra);

	if (node == NULL) {
		fprintf(stderr, "%s: %zu: adding %s as %s: %s\n",
			filename, line_num, extra, path, strerror(errno));
		return -1;
	}

	return 0;
}

static const struct {
	const char *keyword;
	int (*callback)(fstree_t *fs, const char *filename, size_t line_num,
			const char *path, uint16_t mode, uint32_t uid,
			uint32_t gid, const char *extra);
} file_list_hooks[] = {
	{ "dir", add_dir },
	{ "slink", add_slink },
	{ "nod", add_device },
	{ "pipe", add_pipe },
	{ "sock", add_socket },
	{ "file", add_file },
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
	unsigned int mode = 0, uid = 0, gid = 0, x;
	char keyword[16], *path;
	size_t i;

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

	for (; line[i] != '\0'; ++i) {
		/* TODO: escape sequences to support spaces in path */

		if (isspace(line[i]))
			break;
	}

	if (!isspace(line[i]))
		goto fail_ent;

	line[i++] = '\0';
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

		mode = (mode << 3) | (line[i] - '0');

		if (mode > 07777)
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

		if (uid > (0xFFFFFFFF - x) / 10)
			goto fail_ov;

		uid = uid * 10 + x;
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

		if (gid > (0xFFFFFFFF - x) / 10)
			goto fail_ov;

		gid = gid * 10 + x;
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
			return file_list_hooks[i].callback(fs, filename,
							   line_num, path,
							   mode, uid, gid,
							   extra);
		}
	}

	fprintf(stderr, "%s: %zu: unknown entry type '%s'.\n", filename,
		line_num, keyword);
	return -1;
fail_ov:
	msg = "numeric overflow";
	goto fail_ent;
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

int fstree_from_file(fstree_t *fs, const char *filename, const char *rootdir)
{
	FILE *fp = fopen(filename, "rb");
	size_t n, line_num = 0;
	const char *ptr;
	ssize_t ret;
	char *line;

	if (fp == NULL) {
		perror(filename);
		return -1;
	}

	if (rootdir == NULL) {
		ptr = strrchr(filename, '/');

		if (ptr != NULL) {
			line = strndup(filename, ptr - filename);
			if (line == NULL) {
				perror("composing root path");
				return -1;
			}

			if (chdir(line)) {
				perror(line);
				free(line);
				return -1;
			}

			free(line);
			line = NULL;
		}
	} else {
		if (chdir(rootdir)) {
			perror(rootdir);
			return -1;
		}
	}

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

	fclose(fp);
	return 0;
fail_line:
	free(line);
	fclose(fp);
	return -1;
}
