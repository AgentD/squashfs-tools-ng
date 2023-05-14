/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_tree_iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "io/dir_iterator.h"
#include "util/util.h"
#include "sqfs/error.h"

#include <stdlib.h>
#include <string.h>

typedef struct dir_stack_t {
	struct dir_stack_t *next;
	dir_iterator_t *dir;
	char name[];
} dir_stack_t;

typedef struct {
	dir_iterator_t base;

	dir_tree_cfg_t cfg;
	int state;
	dir_stack_t *top;
} dir_tree_iterator_t;

static void pop(dir_tree_iterator_t *it)
{
	if (it->top != NULL) {
		dir_stack_t *ent = it->top;
		it->top = it->top->next;

		sqfs_drop(ent->dir);
		free(ent);
	}
}

static int push(dir_tree_iterator_t *it, const char *name, dir_iterator_t *dir)
{
	dir_stack_t *ent = alloc_flex(sizeof(*ent), 1, strlen(name) + 1);

	if (ent == NULL)
		return SQFS_ERROR_ALLOC;

	strcpy(ent->name, name);
	ent->dir = sqfs_grab(dir);
	ent->next = it->top;
	it->top = ent;
	return 0;
}

static bool should_skip(const dir_tree_iterator_t *dir, const dir_entry_t *ent)
{
	unsigned int type_mask;

	if (!strcmp(ent->name, ".") || !strcmp(ent->name, ".."))
		return true;

	if ((dir->cfg.flags & DIR_SCAN_ONE_FILESYSTEM)) {
		if (ent->dev != ((const dir_iterator_t *)dir)->dev)
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

static dir_entry_t *expand_path(const dir_tree_iterator_t *it, dir_entry_t *ent)
{
	size_t slen = strlen(ent->name) + 1, plen = 0;
	dir_stack_t *sit;
	char *dst;

	for (sit = it->top; sit != NULL; sit = sit->next) {
		if (sit->name[0] != '\0')
			plen += strlen(sit->name) + 1;
	}

	if (it->cfg.prefix != NULL && it->cfg.prefix[0] != '\0')
		plen += strlen(it->cfg.prefix) + 1;

	if (plen > 0) {
		void *new = realloc(ent, sizeof(*ent) + plen + slen);
		if (new == NULL) {
			free(ent);
			return NULL;
		}

		ent = new;
		memmove(ent->name + plen, ent->name, slen);
		dst = ent->name + plen;

		for (sit = it->top; sit != NULL; sit = sit->next) {
			size_t len = strlen(sit->name);
			if (len > 0) {
				*(--dst) = '/';
				dst -= len;
				memcpy(dst, sit->name, len);
			}
		}

		if (it->cfg.prefix != NULL && it->cfg.prefix[0] != '\0') {
			size_t len = strlen(it->cfg.prefix);
			memcpy(ent->name, it->cfg.prefix, len);
			ent->name[len] = '/';
		}
	}

	return ent;
}

static void apply_changes(const dir_tree_iterator_t *it, dir_entry_t *ent)
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

	while (it->top != NULL)
		pop(it);

	free(it);
}

static int next(dir_iterator_t *base, dir_entry_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;
	dir_iterator_t *sub;
	dir_entry_t *ent;
	int ret;
retry:
	*out = NULL;
	sub = NULL;
	ent = NULL;

	if (it->state != 0)
		return it->state;

	for (;;) {
		if (it->top == NULL) {
			ret = 1;
			goto fail;
		}

		ret = it->top->dir->next(it->top->dir, &ent);
		if (ret < 0)
			goto fail;

		if (ret > 0) {
			pop(it);
			continue;
		}

		if (should_skip(it, ent)) {
			free(ent);
			ent = NULL;
			continue;
		}

		break;
	}

	ent = expand_path(it, ent);
	if (ent == NULL) {
		it->state = SQFS_ERROR_ALLOC;
		return it->state;
	}

	apply_changes(it, ent);

	if (S_ISDIR(ent->mode)) {
		if (!(it->cfg.flags & DIR_SCAN_NO_RECURSION)) {
			const char *name = strrchr(ent->name, '/');
			name = (name == NULL) ? ent->name : (name + 1);

			ret = it->top->dir->open_subdir(it->top->dir, &sub);
			if (ret != 0)
				goto fail;

			ret = push(it, name, sub);
			sqfs_drop(sub);
			if (ret != 0)
				goto fail;
		}

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
fail:
	free(ent);
	it->state = ret;
	return it->state;
}

static int read_link(dir_iterator_t *base, char **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->top == NULL) {
		*out = NULL;
		return SQFS_ERROR_NO_ENTRY;
	}

	return it->top->dir->read_link(it->top->dir, out);
}

static int open_subdir(dir_iterator_t *base, dir_iterator_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->top == NULL) {
		*out = NULL;
		return SQFS_ERROR_NO_ENTRY;
	}

	return it->top->dir->open_subdir(it->top->dir, out);
}

static void ignore_subdir(dir_iterator_t *base)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	pop(it);
}

static int open_file_ro(dir_iterator_t *base, istream_t **out)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	if (it->top == NULL) {
		*out = NULL;
		return SQFS_ERROR_NO_ENTRY;
	}

	return it->top->dir->open_file_ro(it->top->dir, out);
}

dir_iterator_t *dir_tree_iterator_create(const char *path,
					 const dir_tree_cfg_t *cfg)
{
	dir_tree_iterator_t *it = calloc(1, sizeof(*it));
	dir_iterator_t *dir;
	int ret;

	if (it == NULL) {
		perror(path);
		return NULL;
	}

	it->cfg = *cfg;

	dir = dir_iterator_create(path);
	if (dir == NULL)
		goto fail;

	ret = push(it, "", dir);
	dir = sqfs_drop(dir);
	if (ret != 0) {
		fprintf(stderr, "%s: out of memory\n", path);
		goto fail;
	}

	sqfs_object_init(it, destroy, NULL);
	((dir_iterator_t *)it)->dev = it->top->dir->dev;
	((dir_iterator_t *)it)->next = next;
	((dir_iterator_t *)it)->read_link = read_link;
	((dir_iterator_t *)it)->open_subdir = open_subdir;
	((dir_iterator_t *)it)->ignore_subdir = ignore_subdir;
	((dir_iterator_t *)it)->open_file_ro = open_file_ro;

	return (dir_iterator_t *)it;
fail:
	free(it);
	return NULL;
}
