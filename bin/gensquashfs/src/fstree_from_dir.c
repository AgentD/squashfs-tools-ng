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

	if (n == root->data.children) {
		root->data.children = n->next;
	} else {
		it = root->data.children;

		while (it != NULL && it->next != n)
			it = it->next;

		if (it != NULL)
			it->next = n->next;
	}

	free(n);
}

static int scan_dir(fstree_t *fs, tree_node_t *root, dir_iterator_t *dir,
		    scan_node_callback cb, void *user, unsigned int flags)
{
	for (;;) {
		dir_entry_t *ent = NULL;
		tree_node_t *n = NULL;
		char *extra = NULL;

		int ret = dir->next(dir, &ent);
		if (ret > 0)
			break;
		if (ret < 0) {
			sqfs_perror("readdir", NULL, ret);
			return -1;
		}

		if (S_ISLNK(ent->mode)) {
			ret = dir->read_link(dir, &extra);
			if (ret) {
				free(ent);
				sqfs_perror("readlink", ent->name, ret);
				return -1;
			}
		}

		if (S_ISDIR(ent->mode) && (flags & DIR_SCAN_NO_DIR)) {
			n = fstree_get_node_by_path(fs, root, ent->name,
						    false, false);
			if (n == NULL) {
				free(ent);
				dir_tree_iterator_skip(dir);
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

			n = fstree_add_generic_at(fs, root, ent->name,
						  &sb, extra);
			if (n == NULL) {
				perror("creating tree node");
				free(extra);
				free(ent);
				return -1;
			}

			ret = (cb == NULL) ? 0 : cb(user, fs, n);
		}

		free(ent);
		free(extra);

		if (ret < 0)
			return -1;

		if (ret > 0) {
			if (S_ISDIR(n->mode))
				dir_tree_iterator_skip(dir);
			discard_node(n->parent, n);
		}
	}

	return 0;
}

int fstree_from_subdir(fstree_t *fs, tree_node_t *root,
		       const char *path, const char *subdir,
		       scan_node_callback cb, void *user,
		       unsigned int flags)
{
	dir_iterator_t *dir;
	dir_tree_cfg_t cfg;
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

	memset(&cfg, 0, sizeof(cfg));
	cfg.flags = flags & ~(DIR_SCAN_NO_DIR);
	cfg.def_mtime = fs->defaults.mtime;

	dir = dir_tree_iterator_create(path, &cfg);
	free(temp);
	if (dir == NULL)
		return -1;

	ret = scan_dir(fs, root, dir, cb, user, flags);
	sqfs_drop(dir);
	return ret;
}

int fstree_from_dir(fstree_t *fs, tree_node_t *root,
		    const char *path, scan_node_callback cb,
		    void *user, unsigned int flags)
{
	return fstree_from_subdir(fs, root, path, NULL, cb, user, flags);
}
