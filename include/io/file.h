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

#ifdef __cplusplus
extern "C" {
#endif

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
