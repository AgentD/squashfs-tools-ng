/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_ISTREAM_H
#define IO_ISTREAM_H

#include "sqfs/predef.h"

/**
 * @struct istream_t
 *
 * @extends sqfs_object_t
 *
 * @brief A sequential, read-only data stream.
 */
typedef struct istream_t {
	sqfs_object_t base;

	size_t buffer_used;
	size_t buffer_offset;
	bool eof;

	sqfs_u8 *buffer;

	int (*precache)(struct istream_t *strm);

	const char *(*get_filename)(struct istream_t *strm);
} istream_t;

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
 * @memberof istream_t
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
SQFS_INTERNAL int istream_get_line(istream_t *strm, char **out,
				   size_t *line_num, int flags);

/**
 * @brief Read data from an input stream
 *
 * @memberof istream_t
 *
 * @param strm A pointer to an input stream.
 * @param data A buffer to read into.
 * @param size The number of bytes to read into the buffer.
 *
 * @return The number of bytes actually read on success, -1 on failure,
 *         0 on end-of-file.
 */
SQFS_INTERNAL sqfs_s32 istream_read(istream_t *strm, void *data, size_t size);

/**
 * @brief Adjust and refill the internal buffer of an input stream
 *
 * @memberof istream_t
 *
 * This function resets the buffer offset of an input stream (moving any unread
 * data up front if it has to) and calls an internal callback of the input
 * stream to fill the rest of the buffer to the extent possible.
 *
 * @param strm A pointer to an input stream.
 *
 * @return 0 on success, -1 on failure.
 */
SQFS_INTERNAL int istream_precache(istream_t *strm);

/**
 * @brief Get the underlying filename of an input stream.
 *
 * @memberof istream_t
 *
 * @param strm The input stream to get the filename from.
 *
 * @return A string holding the underlying filename.
 */
SQFS_INLINE const char *istream_get_filename(istream_t *strm)
{
	return strm->get_filename(strm);
}

/**
 * @brief Skip over a number of bytes in an input stream.
 *
 * @memberof istream_t
 *
 * @param strm A pointer to an input stream.
 * @param size The number of bytes to seek forward.
 *
 * @return Zero on success, -1 on failure.
 */
SQFS_INTERNAL int istream_skip(istream_t *strm, sqfs_u64 size);

#ifdef __cplusplus
}
#endif

#endif /* IO_ISTREAM_H */
