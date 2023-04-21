/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * w32_dir_iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/dir_iterator.h"
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

dir_iterator_t *dir_iterator_create(const char *path)
{
	WCHAR *wpath = NULL, *new = NULL;
	WIN32_FIND_DATAW first_entry;
	dir_iterator_win32_t *it;
	size_t len, newlen;
	HANDLE dirhnd;

	/* convert path to UTF-16, append "\\*" */
	wpath = path_to_windows(path);
	if (wpath == NULL) {
		fprintf(stderr, "%s: allocation failure.\n", path);
		return NULL;
	}

	for (len = 0; wpath[len] != '\0'; ++len)
		;

	newlen = len + 1;

	if (len > 0 && wpath[len - 1] != '\\')
		newlen += 1;

	new = realloc(wpath, sizeof(wpath[0]) * (newlen + 1));
	if (new == NULL) {
		fprintf(stderr, "%s: allocation failure.\n", path);
		free(wpath);
		return NULL;
	}

	wpath = new;

	if (len > 0 && wpath[len - 1] != '\\')
		wpath[len++] = '\\';

	wpath[len++] = '*';
	wpath[len++] = '\0';

	/* get the directory handle AND the first entry */
	dirhnd = FindFirstFileW(wpath, &first_entry);

	if (dirhnd == INVALID_HANDLE_VALUE) {
		w32_perror(path);
		free(wpath);
		return NULL;
	}

	free(wpath);
	wpath = NULL;

	/* wrap it up */
	it = calloc(1, sizeof(*it));
	if (it == NULL) {
		fprintf(stderr, "%s: allocation failure.\n", path);
		FindClose(dirhnd);
		return NULL;
	}

	sqfs_object_init(it, dir_iterator_destroy, NULL);

	((dir_iterator_t *)it)->next = dir_iterator_next;
	((dir_iterator_t *)it)->read_link = dir_iterator_read_link;

	it->is_first = true;
	it->state = 0;
	it->dirhnd = dirhnd;
	it->ent = first_entry;
	return (dir_iterator_t *)it;
}
