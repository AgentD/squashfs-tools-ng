/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_ISTREAM_H
#define IO_ISTREAM_H

#include "sqfs/predef.h"

enum {
	ISTREAM_LINE_LTRIM = 0x01,
	ISTREAM_LINE_RTRIM = 0x02,
	ISTREAM_LINE_SKIP_EMPTY = 0x04,
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Read a line of text from an input stream
 *
 * @memberof sqfs_istream_t
 *
 * The line returned is allocated using malloc and must subsequently be
 * freed when it is no longer needed. The line itself is always null-terminated
 * and never includes the line break characters (LF or CR-LF).
 *
 * If the flag @ref ISTREAM_LINE_LTRIM is set, leading white space characters
 * are removed. If the flag @ref ISTREAM_LINE_RTRIM is set, trailing white space
 * characters are remvoed.
 *
 * If the flag @ref ISTREAM_LINE_SKIP_EMPTY is set and a line is discovered to
 * be empty (after the optional trimming), the function discards the empty line
 * and retries. The given line_num pointer is used to increment the line
 * number.
 *
 * @param strm A pointer to an input stream.
 * @param out Returns a pointer to a line on success.
 * @param line_num This is incremented if lines are skipped.
 * @param flags A combination of flags controling the functions behaviour.
 *
 * @return Zero on success, a negative value on error, a positive value if
 *         end-of-file was reached without reading any data.
 */
SQFS_INTERNAL int istream_get_line(sqfs_istream_t *strm, char **out,
				   size_t *line_num, int flags);

#ifdef __cplusplus
}
#endif

#endif /* IO_ISTREAM_H */
