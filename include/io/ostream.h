/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ostream.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef IO_OSTREAM_H
#define IO_OSTREAM_H

#include "sqfs/predef.h"
#include "io/istream.h"

#if defined(__GNUC__) || defined(__clang__)
#	define PRINTF_ATTRIB(fmt, elipsis)			\
		__attribute__ ((format (printf, fmt, elipsis)))
#else
#	define PRINTF_ATTRIB(fmt, elipsis)
#endif

/**
 * @struct ostream_t
 *
 * @extends sqfs_object_t
 *
 * @brief An append-only data stream.
 */
typedef struct ostream_t {
	sqfs_object_t base;

	int (*append)(struct ostream_t *strm, const void *data, size_t size);

	int (*append_sparse)(struct ostream_t *strm, size_t size);

	int (*flush)(struct ostream_t *strm);

	const char *(*get_filename)(struct ostream_t *strm);
} ostream_t;

enum {
	OSTREAM_OPEN_OVERWRITE = 0x01,
	OSTREAM_OPEN_SPARSE = 0x02,
};

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Append a block of data to an output stream.
 *
 * @memberof ostream_t
 *
 * @param strm A pointer to an output stream.
 * @param data A pointer to the data block to append.
 * @param size The number of bytes to append.
 *
 * @return Zero on success, -1 on failure.
 */
SQFS_INTERNAL int ostream_append(ostream_t *strm, const void *data,
				 size_t size);

/**
 * @brief Append a number of zero bytes to an output stream.
 *
 * @memberof ostream_t
 *
 * If the unerlying implementation supports sparse files, this function can be
 * used to create a "hole". If the implementation does not support it, a
 * fallback is used that just appends a block of zeros manualy.
 *
 * @param strm A pointer to an output stream.
 * @param size The number of zero bytes to append.
 *
 * @return Zero on success, -1 on failure.
 */
SQFS_INTERNAL int ostream_append_sparse(ostream_t *strm, size_t size);

/**
 * @brief Process all pending, buffered data and flush it to disk.
 *
 * @memberof ostream_t
 *
 * If the stream performs some kind of transformation (e.g. transparent data
 * compression), flushing caues the wrapped format to insert a termination
 * token. Only call this function when you are absolutely DONE appending data,
 * shortly before destroying the stream.
 *
 * @param strm A pointer to an output stream.
 *
 * @return Zero on success, -1 on failure.
 */
SQFS_INTERNAL int ostream_flush(ostream_t *strm);

/**
 * @brief Get the underlying filename of a output stream.
 *
 * @memberof ostream_t
 *
 * @param strm The output stream to get the filename from.
 *
 * @return A string holding the underlying filename.
 */
SQFS_INTERNAL const char *ostream_get_filename(ostream_t *strm);

/**
 * @brief Printf like function that appends to an output stream
 *
 * @memberof ostream_t
 *
 * @param strm The output stream to append to.
 * @param fmt A printf style format string.
 *
 * @return The number of characters written on success, -1 on failure.
 */
SQFS_INTERNAL int ostream_printf(ostream_t *strm, const char *fmt, ...)
	PRINTF_ATTRIB(2, 3);

/**
 * @brief Read data from an input stream and append it to an output stream
 *
 * @memberof ostream_t
 *
 * @param out A pointer to an output stream to append to.
 * @param in A pointer to an input stream to read from.
 * @param size The number of bytes to copy over.
 *
 * @return The number of bytes copied on success, -1 on failure,
 *         0 on end-of-file.
 */
SQFS_INTERNAL sqfs_s32 ostream_append_from_istream(ostream_t *out,
						   istream_t *in,
						   sqfs_u32 size);

#ifdef __cplusplus
}
#endif

#endif /* IO_OSTREAM_H */
