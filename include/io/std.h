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
 * @return A pointer to an input stream on success, NULL on failure.
 */
SQFS_INTERNAL sqfs_istream_t *istream_open_stdin(void);

/**
 * @brief Create an output stream that writes to standard output.
 *
 * @memberof sqfs_ostream_t
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL sqfs_ostream_t *ostream_open_stdout(void);

#ifdef __cplusplus
}
#endif

#endif /* IO_STD_H */
