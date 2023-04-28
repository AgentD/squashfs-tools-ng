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

static int scan_dir(fstree_t *fs, tree_node_t *root, dir_iterator_t *dir,
		    scan_node_callback cb, void *user)
{
	for (;;) {
		dir_entry_t *ent = NULL;
		tree_node_t *n = NULL;
		char *extra = NULL;
		struct stat sb;

		int ret = dir->next(dir, &ent);
		if (ret > 0)
			break;
		if (ret < 0) {
			sqfs_perror("readdir", NULL, ret);
			return -1;
		}

		n = fstree_get_node_by_path(fs, root, ent->name, false, true);
		if (n == NULL)
			ret = 1;

		if (ret == 0 && cb != NULL)
			ret = cb(user, root, ent);

		if (ret < 0) {
			free(ent);
			return -1;
		}

		if (ret > 0) {
			if (S_ISDIR(ent->mode))
				dir_tree_iterator_skip(dir);
			free(ent);
			continue;
		}

		if (S_ISLNK(ent->mode)) {
			ret = dir->read_link(dir, &extra);
			if (ret) {
				free(ent);
				sqfs_perror("readlink", ent->name, ret);
				return -1;
			}
		}

		memset(&sb, 0, sizeof(sb));
		sb.st_uid = ent->uid;
		sb.st_gid = ent->gid;
		sb.st_mode = ent->mode;
		sb.st_mtime = clamp_timestamp(ent->mtime);

		n = fstree_add_generic_at(fs, root, ent->name, &sb, extra);
		free(extra);
		free(ent);

		if (n == NULL) {
			perror("creating tree node");
			return -1;
		}
	}

	return 0;
}

int fstree_from_dir(fstree_t *fs, tree_node_t *root, const char *path,
		    scan_node_callback cb, void *user, unsigned int flags)
{
	dir_iterator_t *dir;
	dir_tree_cfg_t cfg;
	int ret;

	memset(&cfg, 0, sizeof(cfg));
	cfg.flags = flags;
	cfg.def_mtime = fs->defaults.mtime;

	dir = dir_tree_iterator_create(path, &cfg);
	if (dir == NULL)
		return -1;

	ret = scan_dir(fs, root, dir, cb, user);
	sqfs_drop(dir);
	return ret;
}
