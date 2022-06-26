/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

typedef struct {
	istream_t base;
	char *path;
	int fd;
	bool eof;

	sqfs_u8 buffer[BUFSZ];
} file_istream_t;

static int file_precache(istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;
	ssize_t ret;
	size_t diff;

	while (strm->buffer_used < sizeof(file->buffer)) {
		diff = sizeof(file->buffer) - strm->buffer_used;

		ret = read(file->fd, strm->buffer + strm->buffer_used, diff);

		if (ret == 0) {
			file->eof = true;
			break;
		}

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(file->path);
			return -1;
		}

		strm->buffer_used += ret;
	}

	return 0;
}

static const char *file_get_filename(istream_t *strm)
{
	file_istream_t *file = (file_istream_t *)strm;

	return file->path;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_istream_t *file = (file_istream_t *)obj;

	if (file->fd != STDIN_FILENO)
		close(file->fd);

	free(file->path);
	free(file);
}

istream_t *istream_open_file(const char *path)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	sqfs_object_t *obj = (sqfs_object_t *)file;
	istream_t *strm = (istream_t *)file;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	file->fd = open(path, O_RDONLY);
	if (file->fd < 0) {
		perror(path);
		goto fail_path;
	}

	strm->buffer = file->buffer;
	strm->precache = file_precache;
	strm->get_filename = file_get_filename;
	obj->destroy = file_destroy;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(file);
	return NULL;
}

istream_t *istream_open_stdin(void)
{
	file_istream_t *file = calloc(1, sizeof(*file));
	sqfs_object_t *obj = (sqfs_object_t *)file;
	istream_t *strm = (istream_t *)file;

	if (file == NULL)
		goto fail;

	file->path = strdup("stdin");
	if (file->path == NULL)
		goto fail;

	file->fd = STDIN_FILENO;
	strm->buffer = file->buffer;
	strm->precache = file_precache;
	strm->get_filename = file_get_filename;
	obj->destroy = file_destroy;
	return strm;
fail:
	perror("creating file wrapper for stdin");
	free(file);
	return NULL;
}
