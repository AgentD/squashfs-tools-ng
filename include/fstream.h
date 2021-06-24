/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * fstream.h
 *
 * Copyright (C) 2019 David Oberhollenzer <goliath@infraroot.at>
 */
#ifndef FSTREAM_H
#define FSTREAM_H

#include "sqfs/predef.h"

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
	OSTREAM_OPEN_OVERWRITE = 0x01,
	OSTREAM_OPEN_SPARSE = 0x02,
};

enum {
	ISTREAM_LINE_LTRIM = 0x01,
	ISTREAM_LINE_RTRIM = 0x02,
	ISTREAM_LINE_SKIP_EMPTY = 0x04,
};

enum {
	/**
	 * @brief Deflate compressor with gzip headers.
	 *
	 * This actually creates a gzip compatible file, including a
	 * gzip header and trailer.
	 */
	FSTREAM_COMPRESSOR_GZIP = 1,

	FSTREAM_COMPRESSOR_XZ = 2,

	FSTREAM_COMPRESSOR_ZSTD = 3,

	FSTREAM_COMPRESSOR_BZIP2 = 4,

	FSTREAM_COMPRESSOR_MIN = 1,
	FSTREAM_COMPRESSOR_MAX = 4,
};

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * @brief Create an output stream that writes to standard output.
 *
 * @memberof ostream_t
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL ostream_t *ostream_open_stdout(void);

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
 * @brief Create an input stream that reads from standard input.
 *
 * @memberof istream_t
 *
 * @return A pointer to an input stream on success, NULL on failure.
 */
SQFS_INTERNAL istream_t *istream_open_stdin(void);

/**
 * @brief Create an output stream that transparently compresses data.
 *
 * @memberof ostream_t
 *
 * This function creates an output stream that transparently compresses all
 * data appended to it and writes the compressed data to an underlying, wrapped
 * output stream.
 *
 * The new stream takes ownership of the wrapped stream and destroys it when
 * the compressor stream is destroyed. If this function fails, the wrapped
 * stream is also destroyed.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param comp_id An identifier describing the compressor to use.
 *
 * @return A pointer to an output stream on success, NULL on failure.
 */
SQFS_INTERNAL ostream_t *ostream_compressor_create(ostream_t *strm,
						   int comp_id);

/**
 * @brief Create an input stream that transparently uncompresses data.
 *
 * @memberof istream_t
 *
 * This function creates an input stream that wraps an underlying input stream
 * that is compressed and transparently uncompresses the data when reading
 * from it.
 *
 * The new stream takes ownership of the wrapped stream and destroys it when
 * the compressor stream is destroyed. If this function fails, the wrapped
 * stream is also destroyed.
 *
 * @param strm A pointer to another stream that should be wrapped.
 * @param comp_id An identifier describing the compressor to use.
 *
 * @return A pointer to an input stream on success, NULL on failure.
 */
SQFS_INTERNAL istream_t *istream_compressor_create(istream_t *strm,
						   int comp_id);

/**
 * @brief Probe the buffered data in an istream to check if it is compressed.
 *
 * @memberof istream_t
 *
 * This function peeks into the internal buffer of an input stream to check
 * for magic signatures of various compressors.
 *
 * @param strm A pointer to an input stream to check
 * @param probe A callback used to check if raw/decoded data matches an
 *              expected format. Returns 0 if not, -1 on failure and +1
 *              on success.
 *
 * @return A compressor ID on success, 0 if no match was found, -1 on failure.
 */
SQFS_INTERNAL int istream_detect_compressor(istream_t *strm,
					    int (*probe)(const sqfs_u8 *data,
							 size_t size));

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
SQFS_INTERNAL const char *istream_get_filename(istream_t *strm);

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

/**
 * @brief Resolve a compressor name to an ID.
 *
 * @param name A compressor name.
 *
 * @return A compressor ID on success, -1 on failure.
 */
SQFS_INTERNAL int fstream_compressor_id_from_name(const char *name);

/**
 * @brief Resolve a id to a  compressor name.
 *
 * @param id A compressor ID.
 *
 * @return A compressor name on success, NULL on failure.
 */
SQFS_INTERNAL const char *fstream_compressor_name_from_id(int id);

/**
 * @brief Check if support for a given compressor has been built in.
 *
 * @param id A compressor ID.
 *
 * @return True if the compressor is supported, false if not.
 */
SQFS_INTERNAL bool fstream_compressor_exists(int id);

#ifdef __cplusplus
}
#endif

#endif /* FSTREAM_H */
