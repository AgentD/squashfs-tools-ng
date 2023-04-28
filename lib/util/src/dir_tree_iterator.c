/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_tree_iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/dir_tree_iterator.h"
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
	if (!strcmp(ent->name, ".") || !strcmp(ent->name, ".."))
		return true;

	if ((dir->cfg.flags & DIR_SCAN_ONE_FILESYSTEM)) {
		if (ent->dev != ((const dir_iterator_t *)dir)->dev)
			return true;
	}

	switch (ent->mode & S_IFMT) {
	case S_IFSOCK:
		return (dir->cfg.flags & DIR_SCAN_NO_SOCK) != 0;
	case S_IFLNK:
		return (dir->cfg.flags & DIR_SCAN_NO_SLINK) != 0;
	case S_IFREG:
		return (dir->cfg.flags & DIR_SCAN_NO_FILE) != 0;
	case S_IFBLK:
		return (dir->cfg.flags & DIR_SCAN_NO_BLK) != 0;
	case S_IFCHR:
		return (dir->cfg.flags & DIR_SCAN_NO_CHR) != 0;
	case S_IFIFO:
		return (dir->cfg.flags & DIR_SCAN_NO_FIFO) != 0;
	default:
		break;
	}

	return false;
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
	dir_stack_t *sit;
	size_t plen;
	int ret;
retry:
	*out = NULL;
	sub = NULL;
	ent = NULL;
	sit = NULL;
	plen = 0;

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

	for (sit = it->top; sit != NULL; sit = sit->next) {
		size_t len = strlen(sit->name);
		if (len > 0)
			plen += len + 1;
	}

	if (plen > 0) {
		size_t slen = strlen(ent->name) + 1;
		void *new = realloc(ent, sizeof(*ent) + plen + slen);
		char *dst;

		if (new == NULL) {
			ret = SQFS_ERROR_ALLOC;
			goto fail;
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
	}

	if (it->cfg.prefix != NULL) {
		size_t slen = strlen(ent->name), len = strlen(it->cfg.prefix);
		void *new = realloc(ent, sizeof(*ent) + len + 1 + slen + 1);

		if (new == NULL) {
			ret = SQFS_ERROR_ALLOC;
			goto fail;
		}

		ent = new;
		memmove(ent->name + len + 1, ent->name, slen + 1);
		memcpy(ent->name, it->cfg.prefix, len);
		ent->name[len] = '/';
		plen += len + 1;
	}

	if (!(it->cfg.flags & DIR_SCAN_KEEP_TIME))
		ent->mtime = it->cfg.def_mtime;

	if (S_ISDIR(ent->mode)) {
		if (!(it->cfg.flags & DIR_SCAN_NO_RECURSION)) {
			ret = it->top->dir->open_subdir(it->top->dir, &sub);
			if (ret != 0)
				goto fail;

			ret = push(it, ent->name + plen, sub);
			sqfs_drop(sub);
			if (ret != 0)
				goto fail;
		}

		if (it->cfg.flags & DIR_SCAN_NO_DIR) {
			free(ent);
			ent = NULL;
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

	return (dir_iterator_t *)it;
fail:
	free(it);
	return NULL;
}

void dir_tree_iterator_skip(dir_iterator_t *base)
{
	dir_tree_iterator_t *it = (dir_tree_iterator_t *)base;

	pop(it);
}
