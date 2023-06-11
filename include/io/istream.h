/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * istream.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_ISTREAM_H
#define IO_ISTREAM_H

#include "sqfs/predef.h"
#include "io/ostream.h"

/**
 * @struct istream_t
 *
 * @extends sqfs_object_t
 *
 * @brief A sequential, read-only data stream.
 */
typedef struct istream_t {
	sqfs_object_t base;

	/**
	 * @brief Peek into the data buffered in an istream
	 *
	 * If the internal buffer is empty, the function tries to fetch more,
	 * which can block. It returns a positive return code if there is no
	 * more data to be read, a negative error code if reading failed. Since
	 * this and other functions can alter the buffer pointer and contents,
	 * do not store the pointers returned here across function calls.
	 *
	 * Higher level functions like @ref istream_read (providing a
	 * Unix read() style API) are built on top of this primitive.
	 *
	 * @param strm A pointer to an istream_t implementation.
	 * @param out Returns a pointer into an internal buffer on success.
	 * @param size Returns the number of bytes available in the buffer.
	 * @param want A number of bytes that the reader would like to have.
	 *             If there is less than this available, the implementation
	 *             can choose to do a blocking precache.
	 *
	 * @return Zero on success, a negative error code on failure,
	 *         a postive number on EOF.
	 */
	int (*get_buffered_data)(struct istream_t *strm, const sqfs_u8 **out,
				 size_t *size, size_t want);

	/**
	 * @brief Mark a section of the internal buffer of an istream as used
	 *
	 * This marks the first `count` bytes of the internal buffer as used,
	 * forcing get_buffered_data to return data afterwards and potentially
	 * try to load more data.
	 *
	 * @param strm A pointer to an istream_t implementation.
	 * @param count The number of bytes used up.
	 */
	void (*advance_buffer)(struct istream_t *strm, size_t count);

	/**
	 * @brief Get the underlying filename of an input stream.
	 *
	 * @param strm The input stream to get the filename from.
	 *
	 * @return A string holding the underlying filename.
	 */
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

/**
 * @brief Dump data from an input stream to an output stream
 *
 * @memberof istream_t
 *
 * @param in A pointer to an input stream to read from.
 * @param out A pointer to an output stream to append to.
 * @param size The number of bytes to copy over.
 *
 * @return The number of bytes copied on success, -1 on failure,
 *         0 on end-of-file.
 */
SQFS_INTERNAL sqfs_s32 istream_splice(istream_t *in, ostream_t *out,
				      sqfs_u32 size);

#ifdef __cplusplus
}
#endif

#endif /* IO_ISTREAM_H */
