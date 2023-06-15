/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.c
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#include "../internal.h"
#include "sqfs/io.h"
#include "sqfs/error.h"

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
			errno = EPIPE;
			return SQFS_ERROR_IO;
		}

		if (ret < 0) {
			if (errno == EINTR)
				continue;
			return SQFS_ERROR_IO;
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
	int ret;

	if (file->sparse_count == 0)
		return 0;

	if (file->flags & SQFS_FILE_OPEN_NO_SPARSE) {
		bufsz = file->sparse_count > 1024 ? 1024 : file->sparse_count;
		buffer = calloc(1, bufsz);
		if (buffer == NULL)
			return SQFS_ERROR_ALLOC;

		while (file->sparse_count > 0) {
			diff = file->sparse_count > (off_t)bufsz ?
				bufsz : (size_t)file->sparse_count;

			ret = write_all(file, buffer, diff);
			if (ret) {
				int temp = errno;
				free(buffer);
				errno = temp;
				return ret;
			}

			file->sparse_count -= diff;
		}

		free(buffer);
	} else {
		if (lseek(file->fd, file->sparse_count, SEEK_CUR) == (off_t)-1)
			return SQFS_ERROR_IO;

		if (ftruncate(file->fd, file->size) != 0)
			return SQFS_ERROR_IO;

		file->sparse_count = 0;
	}

	return 0;
}

static int file_append(sqfs_ostream_t *strm, const void *data, size_t size)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	int ret;

	if (size == 0 || data == NULL) {
		file->sparse_count += size;
		file->size += size;
		return 0;
	}

	ret = realize_sparse(file);
	if (ret)
		return ret;

	return write_all(file, data, size);
}

static int file_flush(sqfs_ostream_t *strm)
{
	file_ostream_t *file = (file_ostream_t *)strm;
	int ret;

	ret = realize_sparse(file);
	if (ret)
		return ret;

	if (fsync(file->fd) != 0) {
		if (errno != EINVAL)
			return SQFS_ERROR_IO;
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
	return ((file_ostream_t *)strm)->path;
}

int ostream_open_handle(sqfs_ostream_t **out, const char *path,
			sqfs_file_handle_t fd, int flags)
{
	file_ostream_t *file = calloc(1, sizeof(*file));
	sqfs_ostream_t *strm = (sqfs_ostream_t *)file;

	*out = NULL;
	if (file == NULL)
		return SQFS_ERROR_ALLOC;

	sqfs_object_init(file, file_destroy, NULL);

	file->path = strdup(path);
	if (file->path == NULL) {
		int temp = errno;
		free(file);
		errno = temp;
		return SQFS_ERROR_ALLOC;
	}

	file->fd = dup(fd);
	if (file->fd < 0) {
		int temp = errno;
		free(file->path);
		free(file);
		errno = temp;
		return SQFS_ERROR_IO;
	}

	close(fd);

	file->flags = flags;
	strm->append = file_append;
	strm->flush = file_flush;
	strm->get_filename = file_get_filename;

	*out = strm;
	return 0;
}

int ostream_open_file(sqfs_ostream_t **out, const char *path, int flags)
{
	sqfs_file_handle_t fd;
	int ret;

	*out = NULL;
	ret = sqfs_open_native_file(&fd, path, flags);
	if (ret)
		return ret;

	ret = ostream_open_handle(out, path, fd, flags);
	if (ret) {
		int temp = errno;
		close(fd);
		errno = temp;
		return ret;
	}

	return 0;
}
