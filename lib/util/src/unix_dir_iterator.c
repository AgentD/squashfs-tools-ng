/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * unix_dir_iterator.c
 *
 * Copyright (C) 2023 David Oberhollenzer <goliath@infraroot.at>
 */
#include "config.h"
#include "util/dir_iterator.h"
#include "util/util.h"
#include "sqfs/error.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

typedef struct {
	dir_iterator_t base;

	struct dirent *ent;
	struct stat sb;
	int state;
	DIR *dir;
} unix_dir_iterator_t;

static void dir_destroy(sqfs_object_t *obj)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)obj;

	closedir(it->dir);
	free(it);
}

static int dir_read_link(dir_iterator_t *base, char **out)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)base;
	size_t size;
	char *str;

	*out = NULL;

	if (it->state != 0 || it->ent == NULL) {
		fputs("[BUG] no entry loaded, cannot readlink\n", stderr);
		return SQFS_ERROR_INTERNAL;
	}

	if (!S_ISLNK(it->sb.st_mode)) {
		fprintf(stderr, "[BUG] %s is not a symlink, cannot readlink\n",
			it->ent->d_name);
		it->state = SQFS_ERROR_INTERNAL;
		return SQFS_ERROR_INTERNAL;
	}

	if ((sizeof(it->sb.st_size) > sizeof(size_t)) &&
	    it->sb.st_size > SIZE_MAX) {
		goto fail_ov;
	}

	if (SZ_ADD_OV((size_t)it->sb.st_size, 1, &size))
		goto fail_ov;

	str = calloc(1, size);
	if (str == NULL) {
		perror(it->ent->d_name);
		it->state = SQFS_ERROR_ALLOC;
		return it->state;
	}

	if (readlinkat(dirfd(it->dir), it->ent->d_name,
		       str, (size_t)it->sb.st_size) < 0) {
		fprintf(stderr, "%s: readlink: %s\n", it->ent->d_name,
			strerror(errno));
		free(str);
		it->state = SQFS_ERROR_INTERNAL;
		return it->state;
	}

	str[it->sb.st_size] = '\0';

	*out = str;
	return it->state;
fail_ov:
	fprintf(stderr, "%s: link target too long\n", it->ent->d_name);
	return SQFS_ERROR_OVERFLOW;
}

static int dir_next(dir_iterator_t *base, dir_entry_t **out)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)base;
	dir_entry_t *decoded;
	size_t len;

	*out = NULL;
	if (it->state != 0)
		return it->state;

	errno = 0;
	it->ent = readdir(it->dir);

	if (it->ent == NULL) {
		if (errno != 0) {
			perror("readdir");
			it->state = SQFS_ERROR_INTERNAL;
		} else {
			it->state = 1;
		}

		return it->state;
	}

	if (fstatat(dirfd(it->dir), it->ent->d_name, &it->sb, AT_SYMLINK_NOFOLLOW)) {
		perror(it->ent->d_name);
		it->state = SQFS_ERROR_INTERNAL;
		return it->state;
	}

	len = strlen(it->ent->d_name);

	decoded = alloc_flex(sizeof(*decoded), 1, len + 1);
	if (decoded == NULL) {
		perror(it->ent->d_name);
		it->state = SQFS_ERROR_ALLOC;
		return it->state;
	}

	memcpy(decoded->name, it->ent->d_name, len);
	decoded->mtime = it->sb.st_mtime;
	decoded->dev = it->sb.st_dev;
	decoded->rdev = it->sb.st_rdev;
	decoded->uid = it->sb.st_uid;
	decoded->gid = it->sb.st_gid;
	decoded->mode = it->sb.st_mode;

	*out = decoded;
	return it->state;
}

static int dir_open_subdir(dir_iterator_t *base, dir_iterator_t **out)
{
	const unix_dir_iterator_t *it = (const unix_dir_iterator_t *)base;
	unix_dir_iterator_t *sub = NULL;
	int fd;

	*out = NULL;

	if (it->state < 0)
		return it->state;

	if (it->state > 0 || it->ent == NULL)
		return SQFS_ERROR_NO_ENTRY;

	fd = openat(dirfd(it->dir), it->ent->d_name, O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		if (errno == ENOTDIR)
			return SQFS_ERROR_NOT_DIR;
		return SQFS_ERROR_IO;
	}

	sub = calloc(1, sizeof(*sub));
	if (sub == NULL)
		goto fail_alloc;

	sub->dir = fdopendir(fd);
	if (sub->dir == NULL)
		goto fail_alloc;

	if (fstat(dirfd(sub->dir), &sub->sb)) {
		free(sub);
		return SQFS_ERROR_IO;
	}

	sqfs_object_init(sub, dir_destroy, NULL);
	((dir_iterator_t *)sub)->dev = sub->sb.st_dev;
	((dir_iterator_t *)sub)->next = dir_next;
	((dir_iterator_t *)sub)->read_link = dir_read_link;
	((dir_iterator_t *)sub)->open_subdir = dir_open_subdir;

	*out = (dir_iterator_t *)sub;
	return 0;
fail_alloc:
	free(sub);
	close(fd);
	return SQFS_ERROR_ALLOC;
}

dir_iterator_t *dir_iterator_create(const char *path)
{
	unix_dir_iterator_t *it = calloc(1, sizeof(*it));

	if (it == NULL) {
		perror(path);
		return NULL;
	}

	it->state = 0;
	it->dir = opendir(path);

	if (it->dir == NULL) {
		perror(path);
		free(it);
		return NULL;
	}

	if (fstat(dirfd(it->dir), &it->sb)) {
		perror(path);
		closedir(it->dir);
		free(it);
		return NULL;
	}

	sqfs_object_init(it, dir_destroy, NULL);
	((dir_iterator_t *)it)->dev = it->sb.st_dev;
	((dir_iterator_t *)it)->next = dir_next;
	((dir_iterator_t *)it)->read_link = dir_read_link;
	((dir_iterator_t *)it)->open_subdir = dir_open_subdir;

	return (dir_iterator_t *)it;
}
