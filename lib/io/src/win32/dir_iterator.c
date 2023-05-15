/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * dir_iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "io/dir_iterator.h"
#include "util/util.h"
#include "sqfs/error.h"

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

#define UNIX_EPOCH_ON_W32 11644473600UL
#define W32_TICS_PER_SEC 10000000UL

typedef struct {
	dir_iterator_t base;

	WIN32_FIND_DATAW ent;
	HANDLE dirhnd;
	int state;
	bool is_first;

	WCHAR path[];
} dir_iterator_win32_t;

static sqfs_s64 w32time_to_unix(const FILETIME *ft)
{
	sqfs_u64 w32ts;

	w32ts = ft->dwHighDateTime;
	w32ts <<= 32UL;
	w32ts |= ft->dwLowDateTime;

	w32ts /= W32_TICS_PER_SEC;

	if (w32ts <= UNIX_EPOCH_ON_W32)
		return -((sqfs_s64)(UNIX_EPOCH_ON_W32 - w32ts));

	return w32ts - UNIX_EPOCH_ON_W32;
}

static int dir_iterator_read_link(dir_iterator_t *it, char **out)
{
	(void)it;
	*out = NULL;
	return SQFS_ERROR_UNSUPPORTED;
}

static int dir_iterator_next(dir_iterator_t *it, dir_entry_t **out)
{
	dir_iterator_win32_t *w32 = (dir_iterator_win32_t *)it;
	dir_entry_t *ent = NULL;
	DWORD length;

	if (w32->state == 0 && !w32->is_first) {
		if (!FindNextFileW(w32->dirhnd, &w32->ent)) {
			if (GetLastError() == ERROR_NO_MORE_FILES) {
				w32->state = 1;
			} else {
				w32->state = SQFS_ERROR_IO;
			}
		}
	}

	w32->is_first = false;

	if (w32->state != 0)
		goto out;

	length = WideCharToMultiByte(CP_UTF8, 0, w32->ent.cFileName,
				     -1, NULL, 0, NULL, NULL);
	if (length <= 0) {
		w32->state = SQFS_ERROR_ALLOC;
		goto out;
	}

	ent = alloc_flex(sizeof(*ent), 1, length + 1);
	if (ent == NULL) {
		w32->state = SQFS_ERROR_ALLOC;
		goto out;
	}

	WideCharToMultiByte(CP_UTF8, 0, w32->ent.cFileName, -1,
			    ent->name, length + 1, NULL, NULL);

	if (w32->ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
		ent->mode = S_IFDIR | 0755;
	} else {
		ent->mode = S_IFREG | 0644;
	}

	ent->mtime = w32time_to_unix(&(w32->ent.ftLastWriteTime));
out:
	*out = ent;
	return w32->state;
}

static void dir_iterator_destroy(sqfs_object_t *obj)
{
	dir_iterator_win32_t *dir = (dir_iterator_win32_t *)obj;

	FindClose(dir->dirhnd);
	free(dir);
}

static void dir_iterator_ignore_subdir(dir_iterator_t *it)
{
	(void)it;
}

static int dir_iterator_open_file_ro(dir_iterator_t *it, istream_t **out)
{
	(void)it;
	*out = NULL;
	return SQFS_ERROR_UNSUPPORTED;
}

static int dir_iterator_read_xattr(dir_iterator_t *it, dir_entry_xattr_t **out)
{
	(void)it;
	*out = NULL;
	return 0;
}

static int dir_iterator_open_subdir(dir_iterator_t *it, dir_iterator_t **out)
{
	const dir_iterator_win32_t *dir = (const dir_iterator_win32_t *)it;
	dir_iterator_win32_t *sub = NULL;
	size_t plen, slen, total;

	*out = NULL;

	if (dir->state != 0)
		return (dir->state > 0) ? SQFS_ERROR_NO_ENTRY : dir->state;

	if (!(dir->ent.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return SQFS_ERROR_NOT_DIR;

	plen = wcslen(dir->path) - 1;
	slen = wcslen(dir->ent.cFileName);
	total = plen + slen + 3;

	sub = alloc_flex(sizeof(*sub), sizeof(WCHAR), total);
	if (sub == NULL)
		return SQFS_ERROR_ALLOC;

	memcpy(sub->path, dir->path, plen * sizeof(WCHAR));
	memcpy(sub->path + plen, dir->ent.cFileName, slen * sizeof(WCHAR));
	sub->path[plen + slen    ] = '\\';
	sub->path[plen + slen + 1] = '*';
	sub->path[plen + slen + 2] = '\0';

	sqfs_object_init(sub, dir_iterator_destroy, NULL);
	((dir_iterator_t *)sub)->next = dir_iterator_next;
	((dir_iterator_t *)sub)->read_link = dir_iterator_read_link;
	((dir_iterator_t *)sub)->open_subdir = dir_iterator_open_subdir;
	((dir_iterator_t *)sub)->ignore_subdir = dir_iterator_ignore_subdir;
	((dir_iterator_t *)sub)->open_file_ro = dir_iterator_open_file_ro;
	((dir_iterator_t *)sub)->read_xattr = dir_iterator_read_xattr;
	sub->is_first = true;
	sub->state = 0;

	sub->dirhnd = FindFirstFileW(sub->path, &sub->ent);
	if (sub->dirhnd == INVALID_HANDLE_VALUE) {
		free(sub);
		return SQFS_ERROR_IO;
	}

	*out = (dir_iterator_t *)sub;
	return 0;
}

dir_iterator_t *dir_iterator_create(const char *path)
{
	dir_iterator_win32_t *it;
	size_t len, newlen;
	WCHAR *wpath = NULL;
	void *new = NULL;

	/* convert path to UTF-16, append "\\*" */
	wpath = path_to_windows(path);
	if (wpath == NULL)
		goto fail_alloc;

	len = wcslen(wpath);
	newlen = len + 1;

	if (len > 0 && wpath[len - 1] != '\\')
		newlen += 1;

	new = realloc(wpath, sizeof(wpath[0]) * (newlen + 1));
	if (new == NULL)
		goto fail_alloc;

	wpath = new;

	if (len > 0 && wpath[len - 1] != '\\')
		wpath[len++] = '\\';

	wpath[len++] = '*';
	wpath[len++] = '\0';

	/* create the sourrounding iterator structure */
	new = realloc(wpath, sizeof(*it) + len * sizeof(wpath[0]));
	if (new == NULL)
		goto fail_alloc;

	it = new;
	wpath = NULL;
	memmove(it->path, new, len * sizeof(wpath[0]));

	/* initialize */
	memset(it, 0, offsetof(dir_iterator_win32_t, path));
	sqfs_object_init(it, dir_iterator_destroy, NULL);

	((dir_iterator_t *)it)->next = dir_iterator_next;
	((dir_iterator_t *)it)->read_link = dir_iterator_read_link;
	((dir_iterator_t *)it)->open_subdir = dir_iterator_open_subdir;
	((dir_iterator_t *)it)->ignore_subdir = dir_iterator_ignore_subdir;
	((dir_iterator_t *)it)->open_file_ro = dir_iterator_open_file_ro;
	((dir_iterator_t *)it)->read_xattr = dir_iterator_read_xattr;
	it->is_first = true;
	it->state = 0;

	/* get the directory handle AND the first entry */
	it->dirhnd = FindFirstFileW(it->path, &it->ent);

	if (it->dirhnd == INVALID_HANDLE_VALUE) {
		w32_perror(path);
		free(it);
		return NULL;
	}

	return (dir_iterator_t *)it;
fail_alloc:
	fprintf(stderr, "%s: allocation failure.\n", path);
	free(wpath);
	return NULL;
}
