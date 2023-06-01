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

#include <sys/stat.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>

typedef struct {
	dir_iterator_t base;

	struct dirent *ent;
	struct stat sb;
	dev_t device;
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
	ssize_t ret;
	size_t size;
	char *str;

	*out = NULL;

	if (it->state < 0)
		return it->state;

	if (it->state > 0 || it->ent == NULL)
		return SQFS_ERROR_NO_ENTRY;

	if ((sizeof(it->sb.st_size) > sizeof(size_t)) &&
	    it->sb.st_size > SIZE_MAX) {
		return SQFS_ERROR_ALLOC;
	}

	if (SZ_ADD_OV((size_t)it->sb.st_size, 1, &size))
		return SQFS_ERROR_ALLOC;

	str = calloc(1, size);
	if (str == NULL)
		return SQFS_ERROR_ALLOC;

	ret = readlinkat(dirfd(it->dir), it->ent->d_name,
			 str, (size_t)it->sb.st_size);
	if (ret < 0) {
		free(str);
		return SQFS_ERROR_IO;
	}

	str[ret] = '\0';

	*out = str;
	return 0;
}

static int dir_next(dir_iterator_t *base, dir_entry_t **out)
{
	unix_dir_iterator_t *it = (unix_dir_iterator_t *)base;

	*out = NULL;
	if (it->state != 0)
		return it->state;

	errno = 0;
	it->ent = readdir(it->dir);

	if (it->ent == NULL) {
		if (errno != 0) {
			it->state = SQFS_ERROR_IO;
		} else {
			it->state = 1;
		}

		return it->state;
	}

	if (fstatat(dirfd(it->dir), it->ent->d_name,
		    &it->sb, AT_SYMLINK_NOFOLLOW)) {
		it->state = SQFS_ERROR_IO;
		return it->state;
	}

	*out = dir_entry_create(it->ent->d_name);
	if ((*out) == NULL) {
		it->state = SQFS_ERROR_ALLOC;
		return it->state;
	}

	(*out)->mtime = it->sb.st_mtime;
	(*out)->dev = it->sb.st_dev;
	(*out)->rdev = it->sb.st_rdev;
	(*out)->uid = it->sb.st_uid;
	(*out)->gid = it->sb.st_gid;
	(*out)->mode = it->sb.st_mode;

	if (S_ISREG(it->sb.st_mode))
		(*out)->size = it->sb.st_size;

	if ((*out)->dev != it->device)
		(*out)->flags |= DIR_ENTRY_FLAG_MOUNT_POINT;

	return it->state;
}

static void dir_ignore_subdir(dir_iterator_t *it)
{
	(void)it;
}

static int dir_open_file_ro(dir_iterator_t *it, istream_t **out)
{
	(void)it;
	*out = NULL;
	return SQFS_ERROR_UNSUPPORTED;
}

static int dir_read_xattr(dir_iterator_t *it, dir_entry_xattr_t **out)
{
	(void)it;
	*out = NULL;
	return 0;
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
	sub->device = sub->sb.st_dev;
	((dir_iterator_t *)sub)->next = dir_next;
	((dir_iterator_t *)sub)->read_link = dir_read_link;
	((dir_iterator_t *)sub)->open_subdir = dir_open_subdir;
	((dir_iterator_t *)sub)->ignore_subdir = dir_ignore_subdir;
	((dir_iterator_t *)sub)->open_file_ro = dir_open_file_ro;
	((dir_iterator_t *)sub)->read_xattr = dir_read_xattr;

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
	it->device = it->sb.st_dev;
	((dir_iterator_t *)it)->next = dir_next;
	((dir_iterator_t *)it)->read_link = dir_read_link;
	((dir_iterator_t *)it)->open_subdir = dir_open_subdir;
	((dir_iterator_t *)it)->ignore_subdir = dir_ignore_subdir;
	((dir_iterator_t *)it)->open_file_ro = dir_open_file_ro;
	((dir_iterator_t *)it)->read_xattr = dir_read_xattr;

	return (dir_iterator_t *)it;
}
