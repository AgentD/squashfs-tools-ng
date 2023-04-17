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

static bool should_skip(const dir_iterator_t *dir, const dir_entry_t *ent,
			unsigned int flags)
{
	if (!strcmp(ent->name, "..") || !strcmp(ent->name, "."))
		return true;

	if ((flags & DIR_SCAN_ONE_FILESYSTEM) && ent->dev != dir->dev)
		return true;

	switch (ent->mode & S_IFMT) {
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
		if (ret < 0)
			goto fail;

		if (should_skip(dir, ent, flags)) {
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

		if (ret > 0) {
			discard_node(root, n);
			continue;
		}

		if (!(flags & DIR_SCAN_NO_RECURSION) && S_ISDIR(n->mode)) {
			if (fstree_from_subdir(fs, n, path, n->name,
					       cb, user, flags)) {
				goto fail;
			}
		}
	}

	sqfs_drop(dir);
	return 0;
fail:
	sqfs_drop(dir);
	return -1;
}

int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       scan_node_callback cb, void *user,
		       unsigned int flags)
{
	size_t plen, slen;
	char *temp = NULL;
	int ret;

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

	ret = scan_dir(fs, root, path, cb, user, flags);
	free(temp);
	return ret;
}

int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, scan_node_callback cb,
		    void *user, unsigned int flags)
{
	return fstree_from_subdir(fs, root, path, NULL, cb, user, flags);
}
