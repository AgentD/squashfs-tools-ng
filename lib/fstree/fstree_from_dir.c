/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "fstree.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       unsigned int flags)
{
	(void)fs; (void)root; (void)path; (void)subdir; (void)flags;
	fputs("Packing a directory is not supported on Windows.\n", stderr);
	return -1;
}

int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, unsigned int flags)
{
	return fstree_from_subdir(fs, root, path, NULL, flags);
}
#else
static int populate_dir(int dir_fd, fstree_t *fs, tree_node_t *root,
			dev_t devstart, unsigned int flags)
{
	char *extra = NULL;
	struct dirent *ent;
	struct stat sb;
	tree_node_t *n;
	int childfd;
	DIR *dir;

	dir = fdopendir(dir_fd);
	if (dir == NULL) {
		perror("fdopendir");
		close(dir_fd);
		return -1;
	}

	/* XXX: fdopendir can dup and close dir_fd internally
	   and still be compliant with the spec. */
	dir_fd = dirfd(dir);

	for (;;) {
		errno = 0;
		ent = readdir(dir);

		if (ent == NULL) {
			if (errno) {
				perror("readdir");
				goto fail;
			}
			break;
		}

		if (!strcmp(ent->d_name, "..") || !strcmp(ent->d_name, "."))
			continue;

		if (fstatat(dir_fd, ent->d_name, &sb, AT_SYMLINK_NOFOLLOW)) {
			perror(ent->d_name);
			goto fail;
		}

		switch (sb.st_mode & S_IFMT) {
		case S_IFSOCK:
			if (flags & DIR_SCAN_NO_SOCK)
				continue;
			break;
		case S_IFLNK:
			if (flags & DIR_SCAN_NO_SLINK)
				continue;
			break;
		case S_IFREG:
			if (flags & DIR_SCAN_NO_FILE)
				continue;
			break;
		case S_IFBLK:
			if (flags & DIR_SCAN_NO_BLK)
				continue;
			break;
		case S_IFCHR:
			if (flags & DIR_SCAN_NO_CHR)
				continue;
			break;
		case S_IFIFO:
			if (flags & DIR_SCAN_NO_FIFO)
				continue;
			break;
		default:
			break;
		}

		if ((flags & DIR_SCAN_ONE_FILESYSTEM) && sb.st_dev != devstart)
			continue;

		if (S_ISLNK(sb.st_mode)) {
			extra = calloc(1, sb.st_size + 1);
			if (extra == NULL)
				goto fail_rdlink;

			if (readlinkat(dir_fd, ent->d_name,
				       extra, sb.st_size) < 0) {
				goto fail_rdlink;
			}

			extra[sb.st_size] = '\0';
		}

		if (!(flags & DIR_SCAN_KEEP_TIME))
			sb.st_mtime = fs->defaults.st_mtime;

		if (S_ISDIR(sb.st_mode) && (flags & DIR_SCAN_NO_DIR)) {
			n = fstree_get_node_by_path(fs, root, ent->d_name,
						    false, false);
			if (n == NULL)
				continue;
		} else {
			n = fstree_mknode(root, ent->d_name,
					  strlen(ent->d_name), extra, &sb);
			if (n == NULL) {
				perror("creating tree node");
				goto fail;
			}
		}

		free(extra);
		extra = NULL;

		if (S_ISDIR(n->mode) && !(flags & DIR_SCAN_NO_RECURSION)) {
			childfd = openat(dir_fd, n->name, O_DIRECTORY |
					 O_RDONLY | O_CLOEXEC);
			if (childfd < 0) {
				perror(n->name);
				goto fail;
			}

			if (populate_dir(childfd, fs, n, devstart, flags))
				goto fail;
		}
	}

	closedir(dir);
	return 0;
fail_rdlink:
	perror("readlink");
fail:
	closedir(dir);
	free(extra);
	return -1;
}

int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       unsigned int flags)
{
	struct stat sb;
	int fd, subfd;

	if (!S_ISDIR(root->mode)) {
		fprintf(stderr,
			"scanning %s/%s into %s: target is not a directory\n",
			path, subdir == NULL ? "" : subdir, root->name);
		return -1;
	}

	fd = open(path, O_DIRECTORY | O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		perror(path);
		return -1;
	}

	if (subdir != NULL) {
		subfd = openat(fd, subdir, O_DIRECTORY | O_RDONLY | O_CLOEXEC);

		if (subfd < 0) {
			fprintf(stderr, "%s/%s: %s\n", path, subdir,
				strerror(errno));
			close(fd);
			return -1;
		}

		close(fd);
		fd = subfd;
	}

	if (fstat(fd, &sb)) {
		fprintf(stderr, "%s/%s: %s\n", path,
			subdir == NULL ? "" : subdir,
			strerror(errno));
		close(fd);
		return -1;
	}

	return populate_dir(fd, fs, root, sb.st_dev, flags);
}

int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, unsigned int flags)
{
	return fstree_from_subdir(fs, root, path, NULL, flags);
}
#endif
