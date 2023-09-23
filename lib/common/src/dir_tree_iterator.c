/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_tree_iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "dir_tree_iterator.h"
#include "util/util.h"
#include "sqfs/error.h"
#include "sqfs/io.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
	sqfs_dir_iterator_t base;

	dir_tree_cfg_t cfg;
	int state;
	sqfs_dir_iterator_t *rec;
} dir_tree_iterator_t;

static bool should_skip(const dir_tree_iterator_t *dir, const sqfs_dir_entry_t *ent)
{
	unsigned int type_mask;

	if ((dir->cfg.flags & DIR_SCAN_ONE_FILESYSTEM)) {
		if (ent->flags & SQFS_DIR_ENTRY_FLAG_MOUNT_POINT)
			return true;
	}

	switch (ent->mode & S_IFMT) {
	case S_IFSOCK: type_mask = DIR_SCAN_NO_SOCK;  break;
	case S_IFLNK:  type_mask = DIR_SCAN_NO_SLINK; break;
	case S_IFREG:  type_mask = DIR_SCAN_NO_FILE;  break;
	case S_IFBLK:  type_mask = DIR_SCAN_NO_BLK;   break;
	case S_IFCHR:  type_mask = DIR_SCAN_NO_CHR;   break;
	case S_IFIFO:  type_mask = DIR_SCAN_NO_FIFO;  break;
	default:       type_mask = 0;                 break;
	}

	return (dir->cfg.flags & type_mask) != 0;
}

static sqfs_dir_entry_t *expand_path(const dir_tree_iterator_t *it, sqfs_dir_entry_t *ent)
{
	if (it->cfg.prefix != NULL && it->cfg.prefix[0] != '\0') {
		size_t plen = strlen(it->cfg.prefix) + 1;
		size_t slen = strlen(ent->name) + 1;
		void *new = realloc(ent, sizeof(*ent) + plen + slen);

		if (new == NULL) {
			free(ent);
			return NULL;
		}

		ent = new;
		memmove(ent->name + plen, ent->name, slen);

		memcpy(ent->name, it->cfg.prefix, plen - 1);
		ent->name[plen - 1] = '/';
	}

	return ent;
}

static void apply_changes(const dir_tree_iterator_t *it, sqfs_dir_entry_t *ent)
{
	if (!(it->cfg.flags & DIR_SCAN_KEEP_TIME))
		ent->mtime = it->cfg.def_mtime;

	if (!(it->cfg.flags & DIR_SCAN_KEEP_UID))
		ent->uid = it->cfg.def_uid;

	if (!(it->cfg.flags & DIR_SCAN_KEEP_GID))
		ent->gid = it->cfg.def_gid;

	if (!(it->cfg.flags & DIR_SCAN_KEEP_MODE)) {
		ent->mode &= ~(07777);
		ent->mode |= it->cfg.def_mode & 07777;
	}
}

/*****************************************************************************/

static void destroy(sqfs_object_t *obj)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)obj;

	sqfs_drop(it->rec);
	free(it);
}

static int next(sqfs_dir_iterator_t *base, sqfs_dir_entry_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;
	sqfs_dir_entry_t *ent;
	int ret;

	if (it->state)
		return it->state;
retry:
	*out = NULL;
	ent = NULL;

	for (;;) {
		ret = it->rec->next(it->rec, &ent);
		if (ret != 0) {
			it->state = ret;
			return ret;
		}

		if (!should_skip(it, ent))
			break;

		if (S_ISDIR(ent->mode))
			it->rec->ignore_subdir(it->rec);
		free(ent);
		ent = NULL;
	}

	ent = expand_path(it, ent);
	if (ent == NULL) {
		it->state = SQFS_ERROR_ALLOC;
		return it->state;
	}

	apply_changes(it, ent);

	if (S_ISDIR(ent->mode)) {
		if (it->cfg.flags & DIR_SCAN_NO_RECURSION)
			it->rec->ignore_subdir(it->rec);

		if (it->cfg.flags & DIR_SCAN_NO_DIR) {
			free(ent);
			goto retry;
		}
	}

	if (it->cfg.name_pattern != NULL) {
		if (it->cfg.flags & DIR_SCAN_MATCH_FULL_PATH) {
			ret = fnmatch(it->cfg.name_pattern,
				      ent->name, FNM_PATHNAME);
		} else {
			const char *name = strrchr(ent->name, '/');
			name = (name == NULL) ? ent->name : (name + 1);

			ret = fnmatch(it->cfg.name_pattern, name, 0);
		}

		if (ret != 0) {
			free(ent);
			goto retry;
		}
	}

	*out = ent;
	return it->state;
}

static int read_link(sqfs_dir_iterator_t *base, char **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->state)
		return it->state;

	return it->rec->read_link(it->rec, out);
}

static int open_subdir(sqfs_dir_iterator_t *base, sqfs_dir_iterator_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->state)
		return it->state;

	return it->rec->open_subdir(it->rec, out);
}

static void ignore_subdir(sqfs_dir_iterator_t *base)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->state == 0)
		it->rec->ignore_subdir(it->rec);
}

static int open_file_ro(sqfs_dir_iterator_t *base, sqfs_istream_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->state)
		return it->state;

	return it->rec->open_file_ro(it->rec, out);
}

static int read_xattr(sqfs_dir_iterator_t *base, sqfs_xattr_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->state)
		return it->state;

	return it->rec->read_xattr(it->rec, out);
}

sqfs_dir_iterator_t *dir_tree_iterator_create(const char *path,
					      const dir_tree_cfg_t *cfg)
{
	dir_tree_iterator_t *it = calloc(1, sizeof(*it));
	sqfs_dir_iterator_t *dir;
	int ret;

	if (it == NULL) {
		perror(path);
		return NULL;
	}

	it->cfg = *cfg;

	ret = sqfs_dir_iterator_create_native(&dir, path, 0);
	if (ret) {
		perror(path);
		goto fail;
	}

	ret = sqfs_dir_iterator_create_recursive(&it->rec, dir);
	sqfs_drop(dir);
	if (ret)
		goto fail_oom;

	if (!(cfg->flags & DIR_SCAN_NO_HARDLINKS)) {
		ret = sqfs_hard_link_filter_create(&dir, it->rec);
		sqfs_drop(it->rec);
		it->rec = dir;
		if (ret)
			goto fail_oom;
	}

	sqfs_object_init(it, destroy, NULL);
	((sqfs_dir_iterator_t *)it)->next = next;
	((sqfs_dir_iterator_t *)it)->read_link = read_link;
	((sqfs_dir_iterator_t *)it)->open_subdir = open_subdir;
	((sqfs_dir_iterator_t *)it)->ignore_subdir = ignore_subdir;
	((sqfs_dir_iterator_t *)it)->open_file_ro = open_file_ro;
	((sqfs_dir_iterator_t *)it)->read_xattr = read_xattr;

	return (sqfs_dir_iterator_t *)it;
fail_oom:
	fprintf(stderr, "%s: out of memory\n", path);
fail:
	free(it);
	return NULL;
}
