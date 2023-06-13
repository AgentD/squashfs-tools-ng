/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * std.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_STD_H
#define IO_STD_H

#include "io/istream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an input stream that reads from standard input.
 *
 * @memberof sqfs_istream_t
 *
 * @param out Returns a pointer to an input stream on success.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_INTERNAL int istream_open_stdin(sqfs_istream_t **out);

/**
 * @brief Create an output stream that writes to standard output.
 *
 * @memberof sqfs_ostream_t
 *
 * @param out Returns a pointer to an output stream on success.
 *
 * @return Zero on success, a negative @ref SQFS_ERROR number on failure
 */
SQFS_INTERNAL int ostream_open_stdout(sqfs_ostream_t **out);

#ifdef __cplusplus
}
#endif

#endif /* IO_STD_H */
