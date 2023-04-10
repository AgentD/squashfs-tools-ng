/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstree_from_dir.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "mkfs.h"

#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static bool should_skip(const char *name, sqfs_u16 mode, unsigned int flags)
{
	if (!strcmp(name, "..") || !strcmp(name, "."))
		return true;

	switch (mode & S_IFMT) {
	case S_IFSOCK:
		return (flags & DIR_SCAN_NO_SOCK) != 0;
	case S_IFLNK:
		return (flags & DIR_SCAN_NO_SLINK) != 0;
	case S_IFREG:
		return (flags & DIR_SCAN_NO_FILE) != 0;
	case S_IFBLK:
		return (flags & DIR_SCAN_NO_BLK) != 0;
	case S_IFCHR:
		return (flags & DIR_SCAN_NO_CHR) != 0;
	case S_IFIFO:
		return (flags & DIR_SCAN_NO_FIFO) != 0;
	default:
		break;
	}

	return false;
}

#if defined(_WIN32) || defined(__WINDOWS__)
static int add_node(fstree_t *fs, tree_node_t *root,
		    scan_node_callback cb, void *user,
		    unsigned int flags,
		    const dir_entry_t *entry)
{
	size_t length = strlen(entry->name);
	tree_node_t *n;

	if (should_skip(entry->name, entry->mode, flags))
		return 0;

	n = calloc(1, sizeof(*n) + length + 1);
	if (n == NULL) {
		fprintf(stderr, "creating tree node: out-of-memory\n");
		return -1;
	}

	n->mode = entry->mode;
	n->name = (char *)n->payload;
	memcpy(n->name, entry->name, length);

	if (cb != NULL) {
		int ret = cb(user, fs, n);

		if (ret != 0) {
			free(n);
			return ret < 0 ? ret : 0;
		}
	}

	if (flags & DIR_SCAN_KEEP_TIME) {
		if (entry->mtime < 0) {
			n->mod_time = 0;
		} else if (entry->mtime > 0x0FFFFFFFFLL) {
			n->mod_time = 0xFFFFFFFF;
		} else {
			n->mod_time = entry->mtime;
		}
	} else {
		n->mod_time = fs->defaults.mtime;
	}

	fstree_insert_sorted(root, n);
	return 0;
}

static int scan_dir(fstree_t *fs, tree_node_t *root,
		    const char *path,
		    scan_node_callback cb, void *user,
		    unsigned int flags)
{
	dir_iterator_t *it = dir_iterator_create(path);

	if (it == NULL)
		return -1;

	for (;;) {
		dir_entry_t *ent;
		int ret;

		ret = it->next(it, &ent);
		if (ret > 0)
			break;
		if (ret < 0) {
			sqfs_perror(path, "reading directory entry", ret);
			sqfs_drop(it);
			return -1;
		}

		ret = add_node(fs, root, cb, user, flags, ent);
		free(ent);
		if (ret != 0) {
			sqfs_drop(it);
			return -1;
		}
	}

	sqfs_drop(it);
	return 0;
}

int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       scan_node_callback cb, void *user,
		       unsigned int flags)
{
	size_t plen, slen;
	char *temp = NULL;
	tree_node_t *n;

	plen = strlen(path);
	slen = subdir == NULL ? 0 : strlen(subdir);

	if (slen > 0) {
		temp = calloc(1, plen + 1 + slen + 1);
		if (temp == NULL) {
			fprintf(stderr, "%s/%s: allocation failure.\n",
				path, subdir);
			return -1;
		}

		memcpy(temp, path, plen);
		temp[plen] = '/';
		memcpy(temp + plen + 1, subdir, slen);
		temp[plen + 1 + slen] = '\0';

		path = temp;
	}

	if (scan_dir(fs, root, path, cb, user, flags))
		goto fail;

	if (flags & DIR_SCAN_NO_RECURSION) {
		free(temp);
		return 0;
	}

	for (n = root->data.dir.children; n != NULL; n = n->next) {
		if (!S_ISDIR(n->mode))
			continue;

		if (fstree_from_subdir(fs, n, path, n->name, cb, user, flags))
			goto fail;
	}

	free(temp);
	return 0;
fail:
	free(temp);
	return -1;

}
#else
static void discard_node(tree_node_t *root, tree_node_t *n)
{
	tree_node_t *it;

	if (n == root->data.dir.children) {
		root->data.dir.children = n->next;
	} else {
		it = root->data.dir.children;

		while (it != NULL && it->next != n)
			it = it->next;

		if (it != NULL)
			it->next = n->next;
	}

	free(n);
}

static char *read_link(const struct stat *sb, int dir_fd, const char *name)
{
	size_t size;
	char *out;

	if ((sizeof(sb->st_size) > sizeof(size_t)) && sb->st_size > SIZE_MAX) {
		errno = EOVERFLOW;
		return NULL;
	}

	if (SZ_ADD_OV((size_t)sb->st_size, 1, &size)) {
		errno = EOVERFLOW;
		return NULL;
	}

	out = calloc(1, size);
	if (out == NULL)
		return NULL;

	if (readlinkat(dir_fd, name, out, (size_t)sb->st_size) < 0) {
		int temp = errno;
		free(out);
		errno = temp;
		return NULL;
	}

	out[sb->st_size] = '\0';
	return out;
}

static int populate_dir(const char *path, fstree_t *fs, tree_node_t *root,
			scan_node_callback cb, void *user, unsigned int flags)
{
	char *extra = NULL;
	dev_t devstart = 0;
	struct dirent *ent;
	struct stat sb;
	tree_node_t *n;
	DIR *dir;
	int ret;

	dir = opendir(path);
	if (dir == NULL) {
		perror(path);
		return -1;
	}

	if (flags & DIR_SCAN_ONE_FILESYSTEM) {
		if (fstat(dirfd(dir), &sb)) {
			perror(path);
			goto fail;
		}
		devstart = sb.st_dev;
	}

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

		if (fstatat(dirfd(dir), ent->d_name,
			    &sb, AT_SYMLINK_NOFOLLOW)) {
			perror(ent->d_name);
			goto fail;
		}

		if (should_skip(ent->d_name, sb.st_mode, flags))
			continue;

		if ((flags & DIR_SCAN_ONE_FILESYSTEM) && sb.st_dev != devstart)
			continue;

		if (S_ISLNK(sb.st_mode)) {
			extra = read_link(&sb, dirfd(dir), ent->d_name);
			if (extra == NULL) {
				perror("readlink");
				goto fail;
			}
		}

		if (!(flags & DIR_SCAN_KEEP_TIME))
			sb.st_mtime = fs->defaults.mtime;

		if (S_ISDIR(sb.st_mode) && (flags & DIR_SCAN_NO_DIR)) {
			n = fstree_get_node_by_path(fs, root, ent->d_name,
						    false, false);
			if (n == NULL)
				continue;

			ret = 0;
		} else {
			n = fstree_mknode(root, ent->d_name,
					  strlen(ent->d_name), extra, &sb);
			if (n == NULL) {
				perror("creating tree node");
				goto fail;
			}

			ret = (cb == NULL) ? 0 : cb(user, fs, n);
		}

		free(extra);
		extra = NULL;

		if (ret < 0)
			goto fail;

		if (ret > 0)
			discard_node(root, n);
	}

	closedir(dir);
	return 0;
fail:
	closedir(dir);
	free(extra);
	return -1;
}

int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       scan_node_callback cb, void *user,
		       unsigned int flags)
{
	size_t plen, slen;
	char *temp = NULL;
	tree_node_t *n;

	if (!S_ISDIR(root->mode)) {
		fprintf(stderr,
			"scanning %s/%s into %s: target is not a directory\n",
			path, subdir == NULL ? "" : subdir, root->name);
		return -1;
	}

	plen = strlen(path);
	slen = subdir == NULL ? 0 : strlen(subdir);

	if (slen > 0) {
		temp = calloc(1, plen + 1 + slen + 1);
		if (temp == NULL) {
			fprintf(stderr, "%s/%s: allocation failure.\n",
				path, subdir);
			return -1;
		}

		memcpy(temp, path, plen);
		temp[plen] = '/';
		memcpy(temp + plen + 1, subdir, slen);
		temp[plen + 1 + slen] = '\0';

		path = temp;
	}

	if (populate_dir(path, fs, root, cb, user, flags))
		goto fail;

	if (flags & DIR_SCAN_NO_RECURSION) {
		free(temp);
		return 0;
	}

	for (n = root->data.dir.children; n != NULL; n = n->next) {
		if (!S_ISDIR(n->mode))
			continue;

		if (fstree_from_subdir(fs, n, path, n->name, cb, user, flags))
			goto fail;
	}

	free(temp);
	return 0;
fail:
	free(temp);
	return -1;
}
#endif

int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, scan_node_callback cb,
		    void *user, unsigned int flags)
{
	return fstree_from_subdir(fs, root, path, NULL, cb, user, flags);
}
