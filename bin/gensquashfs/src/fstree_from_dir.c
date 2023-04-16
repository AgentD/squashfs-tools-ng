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

static sqfs_u32 clamp_timestamp(sqfs_s64 ts)
{
	if (ts < 0)
		return 0;
	if (ts > 0x0FFFFFFFFLL)
		return 0xFFFFFFFF;
	return ts;
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
		n->mod_time = clamp_timestamp(entry->mtime);
	} else {
		n->mod_time = fs->defaults.mtime;
	}

	fstree_insert_sorted(root, n);
	return 0;
}

static int scan_dir(fstree_t *fs, tree_node_t *root, const char *path,
		    scan_node_callback cb, void *user, unsigned int flags)
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

static int scan_dir(fstree_t *fs, tree_node_t *root, const char *path,
		    scan_node_callback cb, void *user, unsigned int flags)
{
	dir_iterator_t *dir;

	dir = dir_iterator_create(path);
	if (dir == NULL)
		return -1;

	for (;;) {
		dir_entry_t *ent = NULL;
		tree_node_t *n = NULL;
		char *extra = NULL;

		int ret = dir->next(dir, &ent);
		if (ret > 0)
			break;
		if (ret < 0) {
			sqfs_drop(dir);
			return -1;
		}

		if (should_skip(ent->name, ent->mode, flags)) {
			free(ent);
			continue;
		}

		if ((flags & DIR_SCAN_ONE_FILESYSTEM) && ent->dev != dir->dev) {
			free(ent);
			continue;
		}

		if (S_ISLNK(ent->mode)) {
			if (dir->read_link(dir, &extra)) {
				free(ent);
				return -1;
			}
		}

		if (!(flags & DIR_SCAN_KEEP_TIME))
			ent->mtime = fs->defaults.mtime;

		if (S_ISDIR(ent->mode) && (flags & DIR_SCAN_NO_DIR)) {
			n = fstree_get_node_by_path(fs, root, ent->name,
						    false, false);
			if (n == NULL) {
				free(ent);
				continue;
			}

			ret = 0;
		} else {
			struct stat sb;

			memset(&sb, 0, sizeof(sb));
			sb.st_uid = ent->uid;
			sb.st_gid = ent->gid;
			sb.st_mode = ent->mode;
			sb.st_mtime = clamp_timestamp(ent->mtime);

			n = fstree_mknode(root, ent->name,
					  strlen(ent->name), extra, &sb);
			if (n == NULL) {
				perror("creating tree node");
				free(extra);
				free(ent);
				goto fail;
			}

			ret = (cb == NULL) ? 0 : cb(user, fs, n);
		}

		free(ent);
		free(extra);

		if (ret < 0)
			goto fail;

		if (ret > 0)
			discard_node(root, n);
	}

	sqfs_drop(dir);
	return 0;
fail:
	sqfs_drop(dir);
	return -1;
}
#endif

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

int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, scan_node_callback cb,
		    void *user, unsigned int flags)
{
	return fstree_from_subdir(fs, root, path, NULL, cb, user, flags);
}
