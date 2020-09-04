/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"

typedef struct {
	ostream_t base;
	char *path;
	int fd;
} file_ostream_t;

static int file_append(ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	ssize_t ret;

	while (size > 0) {
		ret = write(file->fd, data, size);

		if (ret == 0) {
			fprintf(stderr, "%s: truncated data write.\n",
				file->path);
			return -1;
		}

		if (ret < 0) {
			if (errno == EINTR)
				continue;

			perror(file->path);
			return -1;
		}

		size -= ret;
		data = (const char *)data + ret;
	}

	return 0;
}

static int file_append_sparse(ostream_t *strm, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	if (lseek(file->fd, size, SEEK_CUR) == (off_t)-1) {
		perror(file->path);
		return -1;
	}

	return 0;
}

static int file_flush(ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	if (fsync(file->fd) != 0) {
		perror(file->path);
		return -1;
	}

	return 0;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_ostream_t *file = (file_ostream_t *)obj;

	if (file->fd != STDOUT_FILENO)
		close(file->fd);

	free(file->path);
	free(file);
}

static const char *file_get_filename(ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	return file->path;
}

ostream_t *ostream_open_file(const char *path, int flags)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	sqfs_object_t *obj = (sqfs_object_t *)file;
	ostream_t *strm = (ostream_t *)file;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	if (flags & OSTREAM_OPEN_OVERWRITE) {
		file->fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	} else {
		file->fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	}

	if (file->fd < 0) {
		perror(path);
		goto fail_path;
	}

	if (flags & OSTREAM_OPEN_SPARSE)
		strm->append_sparse = file_append_sparse;

	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;
	obj->destroy = file_destroy;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(file);
	return NULL;
}

ostream_t *ostream_open_stdout(void)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	sqfs_object_t *obj = (sqfs_object_t *)file;
	ostream_t *strm = (ostream_t *)file;

	if (file == NULL)
		goto fail;

	file->path = strdup("stdout");
	if (file->path == NULL)
		goto fail;

	file->fd = STDOUT_FILENO;
	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;
	obj->destroy = file_destroy;
	return strm;
fail:
	perror("creating file wrapper for stdout");
	free(file);
	return NULL;
}
