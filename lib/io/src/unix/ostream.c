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

	off_t sparse_count;
	off_t size;
} file_ostream_t;

static int file_append(ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	ssize_t ret;

	if (size == 0)
		return 0;

	if (file->sparse_count > 0) {
		if (lseek(file->fd, file->sparse_count, SEEK_CUR) == (off_t)-1)
			goto fail_errno;

		file->sparse_count = 0;
	}

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
			goto fail_errno;
		}

		file->size += ret;
		size -= ret;
		data = (const char *)data + ret;
	}

	return 0;
fail_errno:
	perror(file->path);
	return -1;
}

static int file_append_sparse(ostream_t *strm, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	file->sparse_count += size;
	file->size += size;
	return 0;
}

static int file_flush(ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	if (file->sparse_count > 0) {
		if (ftruncate(file->fd, file->size) != 0)
			goto fail;
	}

	if (fsync(file->fd) != 0) {
		if (errno == EINVAL)
			return 0;
		goto fail;
	}

	return 0;
fail:
	perror(file->path);
	return -1;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_ostream_t *file = (file_ostream_t *)obj;

	close(file->fd);
	free(file->path);
	free(file);
}

static const char *file_get_filename(ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	return file->path;
}

ostream_t *ostream_open_handle(const char *path, int fd, int flags)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	ostream_t *strm = (ostream_t *)file;

	if (file == NULL) {
		perror(path);
		return NULL;
	}

	sqfs_object_init(file, file_destroy, NULL);

	file->path = strdup(path);
	if (file->path == NULL) {
		perror(path);
		goto fail_free;
	}

	file->fd = dup(fd);
	if (file->fd < 0) {
		perror(path);
		goto fail_path;
	}

	close(fd);

	if (flags & OSTREAM_OPEN_SPARSE)
		strm->append_sparse = file_append_sparse;

	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;
	return strm;
fail_path:
	free(file->path);
fail_free:
	free(file);
	return NULL;
}

ostream_t *ostream_open_file(const char *path, int flags)
{
	ostream_t *out;
	int fd;

	if (flags & OSTREAM_OPEN_OVERWRITE) {
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	} else {
		fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	}

	out = ostream_open_handle(path, fd, flags);
	if (out == NULL)
		close(fd);

	return out;
}

ostream_t *ostream_open_stdout(void)
{
	return ostream_open_handle("stdout", STDOUT_FILENO, 0);
}
