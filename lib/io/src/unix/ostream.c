/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"
#include "sqfs/io.h"

typedef struct {
	sqfs_ostream_t base;
	char *path;
	int flags;
	int fd;

	off_t sparse_count;
	off_t size;
} file_ostream_t;

static int write_all(file_ostream_t *file, const sqfs_u8 *data, size_t size)
{
	while (size > 0) {
		ssize_t ret = write(file->fd, data, size);

		if (ret == 0) {
			fprintf(stderr, "%s: truncated write.\n", file->path);
			return -1;
		}

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			perror(file->path);
			return -1;
		}

		file->size += ret;
		size -= ret;
		data += ret;
	}

	return 0;
}

static int realize_sparse(file_ostream_t *file)
{
	unsigned char *buffer;
	size_t diff, bufsz;

	if (file->sparse_count == 0)
		return 0;

	if (file->flags & SQFS_FILE_OPEN_NO_SPARSE) {
		bufsz = file->sparse_count > 1024 ? 1024 : file->sparse_count;
		buffer = calloc(1, bufsz);
		if (buffer == NULL)
			goto fail;

		while (file->sparse_count > 0) {
			diff = file->sparse_count > (off_t)bufsz ?
				bufsz : (size_t)file->sparse_count;

			if (write_all(file, buffer, diff)) {
				free(buffer);
				return -1;
			}

			file->sparse_count -= diff;
		}

		free(buffer);
	} else {
		if (lseek(file->fd, file->sparse_count, SEEK_CUR) == (off_t)-1)
			goto fail;

		if (ftruncate(file->fd, file->size) != 0)
			goto fail;

		file->sparse_count = 0;
	}

	return 0;
fail:
	perror(file->path);
	return -1;
}

static int file_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	if (size == 0)
		return 0;

	if (data == NULL) {
		file->sparse_count += size;
		file->size += size;
		return 0;
	}

	if (realize_sparse(file))
		return -1;

	return write_all(file, data, size);
}

static int file_flush(sqfs_ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	if (realize_sparse(file))
		return -1;

	if (fsync(file->fd) != 0) {
		if (errno == EINVAL)
			return 0;
		perror(file->path);
		return -1;
	}

	return 0;
}

static void file_destroy(sqfs_object_t *obj)
{
	file_ostream_t *file = (file_ostream_t *)obj;

	close(file->fd);
	free(file->path);
	free(file);
}

static const char *file_get_filename(sqfs_ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;

	return file->path;
}

sqfs_ostream_t *ostream_open_handle(const char *path, int fd, int flags)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	sqfs_ostream_t *strm = (sqfs_ostream_t *)file;

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

	file->flags = flags;
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

sqfs_ostream_t *ostream_open_file(const char *path, int flags)
{
	sqfs_ostream_t *out;
	int fd;

	if (flags & SQFS_FILE_OPEN_OVERWRITE) {
		fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	} else {
		fd = open(path, O_WRONLY | O_CREAT | O_EXCL, 0644);
	}

	out = ostream_open_handle(path, fd, flags);
	if (out == NULL)
		close(fd);

	return out;
}

sqfs_ostream_t *ostream_open_stdout(void)
{
	return ostream_open_handle("stdout", STDOUT_FILENO,
				   SQFS_FILE_OPEN_NO_SPARSE);
}
