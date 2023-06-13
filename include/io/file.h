/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_FILE_H
#define IO_FILE_H

#include "sqfs/io.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an input stream for an OS native file handle.
 *
 * @memberof sqfs_istream_t
 *
 * The functions takes up ownership of the file handle and takes care
 * of cleaning it up. On failure, the handle remains usable, and ownership
 * remains with the caller.
 *
 * @param out Returns a pointer to an input stream on success.
 * @param path The name to associate with the handle.
 * @param fd A native file handle.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_INTERNAL
int istream_open_handle(sqfs_istream_t **out, const char *path,
			sqfs_file_handle_t fd);

/**
 * @brief Create an output stream that writes to an OS native file handle.
 *
 * @memberof sqfs_ostream_t
 *
 * If the flag SQFS_FILE_OPEN_NO_SPARSE is set, the underlying implementation
 * always writes chunks of zero bytes when passing a NULL pointer to append.
 * Otherwise, it tries to use seek/truncate style APIs to create sparse output
 * files.
 *
 * @param out Returns a pointer to an output stream on success.
 * @param path The name to associate with the handle.
 * @param fd A native file handle.
 * @param flags A combination of flags.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_INTERNAL int ostream_open_handle(sqfs_ostream_t **out, const char *path,
				      sqfs_file_handle_t hnd, int flags);

/**
 * @brief Create an input stream that reads from a file.
 *
 * @memberof sqfs_istream_t
 *
 * @param out Returns a pointer to an input stream on success.
 * @param path A path to the file to open or create.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_INTERNAL int istream_open_file(sqfs_istream_t **out, const char *path);

/**
 * @brief Create an output stream that writes to a file.
 *
 * @memberof sqfs_ostream_t
 *
 * If the file does not yet exist, it is created. If it does exist this
 * function fails, unless the flag SQFS_FILE_OPEN_OVERWRITE is set, in which
 * case the file is opened and its contents are discarded.
 *
 * If the flag SQFS_FILE_OPEN_NO_SPARSE is set, the underlying implementation
 * always writes chunks of zero bytes when passing a NULL pointer to append.
 * Otherwise, it tries to use seek/truncate style APIs to create sparse output
 * files.
 *
 * @param out Returns a pointer to an output stream on success.
 * @param path A path to the file to open or create.
 * @param flags A combination of flags controling how to open/create the file.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_INTERNAL int ostream_open_file(sqfs_ostream_t **out,
				    const char *path, int flags);

#ifdef __cplusplus
}
#endif

#endif /* IO_FILE_H */
