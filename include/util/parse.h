/* SPDX-License-Identifier: LGPL-3.0-or-later */
/*
 * parse.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef UTIL_PARSE_H
#define UTIL_PARSE_H

#include "sqfs/predef.h"

enum {
	ISTREAM_LINE_LTRIM = 0x01,
	ISTREAM_LINE_RTRIM = 0x02,
	ISTREAM_LINE_SKIP_EMPTY = 0x04,
};

enum {
	SPLIT_LINE_OK = 0,
	SPLIT_LINE_ALLOC = -1,
	SPLIT_LINE_UNMATCHED_QUOTE = -2,
	SPLIT_LINE_ESCAPE = -3,
};

typedef struct {
	size_t count;
	char *args[];
} split_line_t;

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

/**
 * @brief Split a line of special character separated tokens
 *
 * The underlying string is modified, replacing sequences of separator
 * characters with a single null byte and compacting the string. Every
 * occourance of a termianted string is recorded in the returned structure.
 *
 * @param line A modifyable buffer holding a line
 * @param len The maximum length of the string in the buffer to process
 * @param sep A string of valid separator caracaters
 * @param out Returns the token list, free this with free()
 *
 * @return Zero on success, a negative SPLIT_LINE_* error code on failure
 */
SQFS_INTERNAL int split_line(char *line, size_t len,
			     const char *sep, split_line_t **out);

/**
 * @brief Remove the first N components of a tokenized line
 *
 * @param sep A successfully split up line
 * @param count Number of components to remove from the front
 */
SQFS_INTERNAL void split_line_remove_front(split_line_t *sep, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_PARSE_H */
