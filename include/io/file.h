/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * file.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_FILE_H
#define IO_FILE_H

#include "io/istream.h"
#include "io/ostream.h"

#if defined(_WIN32) || defined(__WINDOWS__)
#include <handleapi.h>

typedef HANDLE os_file_t;
#else
typedef int os_file_t;
#endif

enum {
	OSTREAM_OPEN_OVERWRITE = 0x01,
	OSTREAM_OPEN_SPARSE = 0x02,
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an input stream for an OS native file handle.
 *
 * @memberof istream_t
 *
 * The functions takes up ownership of the file handle and takes care
 * of cleaning it up. On failure, the handle remains usable, and ownership
 * remains with the caller.
 *
 * @param path The name to associate with the handle.
 * @param fd A native file handle.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL istream_t *istream_open_handle(const char *path, os_file_t fd);

/**
 * @brief Create an output stream that writes to an OS native file handle.
 *
 * @memberof ostream_t
 *
 * If the flag OSTREAM_OPEN_SPARSE is set, the underlying implementation tries
 * to use seek/truncate style API to create sparse output files. If the flag
 * is not set, holes will always be filled with zero bytes.
 *
 * @param path The name to associate with the handle.
 * @param fd A native file handle.
 * @param flags A combination of flags.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL ostream_t *ostream_open_handle(const char *path, os_file_t hnd,
					     int flags);

/**
 * @brief Create an input stream that reads from a file.
 *
 * @memberof istream_t
 *
 * @param path A path to the file to open or create.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL istream_t *istream_open_file(const char *path);

/**
 * @brief Create an output stream that writes to a file.
 *
 * @memberof ostream_t
 *
 * If the file does not yet exist, it is created. If it does exist this
 * function fails, unless the flag OSTREAM_OPEN_OVERWRITE is set, in which
 * case the file is opened and its contents are discarded.
 *
 * If the flag OSTREAM_OPEN_SPARSE is set, the underlying implementation tries
 * to support sparse output files. If the flag is not set, holes will always
 * be filled with zero bytes.
 *
 * @param path A path to the file to open or create.
 * @param flags A combination of flags controling how to open/create the file.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL ostream_t *ostream_open_file(const char *path, int flags);

#ifdef __cplusplus
}
#endif

#endif /* IO_FILE_H */
